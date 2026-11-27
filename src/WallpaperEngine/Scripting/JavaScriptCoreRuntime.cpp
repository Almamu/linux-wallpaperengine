// Implements the embedding API used by SceneScript against JavaScriptCore.
// Keeping this boundary lets the existing adapters and the QuickJS fallback
// share one implementation. Authored modules are lowered by ScriptEngine.
#include "JavaScriptCoreRuntime.h"
#include "quickjs.h"
#include <JavaScriptCore/JavaScript.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <glib.h>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace WTF {
void initializeMainThread () __attribute__ ((weak));
}
extern "C" uint32_t webkitThreadSequence ()
    __asm__ ("_ZN3WTF10ThreadLike15currentSequenceEv") __attribute__ ((weak));

namespace {
struct RuntimeLoop {
    std::unique_ptr<GMainContext, decltype(&g_main_context_unref)> context { nullptr, g_main_context_unref };
    std::vector<JSRuntime*> runtimes;
    std::atomic_bool finalizersPending = false;
    bool dispatchWhileActive = false;
};

thread_local RuntimeLoop runtimeLoop;

bool initializeMainRunLoop () {
    // Standalone GTK JSC does not initialize the WTF main loop that its full-GC
    // timer's memory pressure handler needs. The upstream JSC shell does this
    // before creating a VM. Keep dispatch optional across library versions.
    static const bool initialized = [] {
	if (gettid () != getpid () || !WTF::initializeMainThread || !webkitThreadSequence) return false;
	// WTF requires the main thread to be the first thread it registered.
	if (webkitThreadSequence () != 1) return false;
	WTF::initializeMainThread ();
	return true;
    }();
    return initialized;
}
}

struct Native;
struct Holder {
    int refs = 1;
    JSContext* ctx;
    JSValueRef value;
    Native* native;
};
struct Class {
    JSClassDef def {};
    JSClassExoticMethods exotic {};
    JSClassRef klass = nullptr;
};
struct JSModuleDef {
    std::string name;
    bool initialized = false;
    JSModuleInitFunc* init;
    std::unordered_map<std::string, JSValue> exports;
};
struct JSRuntime {
    RuntimeLoop& loop;
    JSContextGroupRef group = nullptr;
    std::unordered_map<JSClassID, std::unique_ptr<Class>> classes;
    std::vector<JSContext*> contexts;
    std::unordered_set<Native*> natives;
    std::unordered_set<Holder*> holders;
    std::mutex mutex;
    std::vector<Native*> collected;
    JSClassID nextClass = 100;
};
struct JSContext {
    JSRuntime* rt;
    JSGlobalContextRef jsc;
    JSValue exception = JS_UNINITIALIZED;
    std::unordered_map<JSClassID, JSValue> protos;
    std::vector<std::string> atoms { std::string () };
    std::unordered_map<std::string, JSAtom> atomMap;
    std::vector<std::unique_ptr<JSModuleDef>> modules;
    JSObjectRef defineProperty = nullptr, hasOwnProperty = nullptr, stringify = nullptr, functionProto = nullptr;
    JSObjectRef objectProto = nullptr;
    JSObjectRef syntaxError = nullptr;
    JSObjectRef uint8ArrayConstructor = nullptr, typedArrayValues = nullptr;
};
struct Native {
    JSContext* ctx;
    std::atomic<JSRuntime*> rt;
    JSClassID id = 0;
    void* opaque = nullptr;
    JSCFunctionType function {};
    JSCFunctionData* dataFunction = nullptr;
    JSCFunctionEnum kind = JS_CFUNC_generic;
    int magic = 0;
    bool queryingOwnProperty = false;
    std::vector<JSValue> data;
};
struct Str {
    JSStringRef value;
    explicit Str (std::string_view input) {
	std::vector<JSChar> utf16;
	utf16.reserve (input.size ());
	for (size_t offset = 0; offset < input.size ();) {
	    gunichar code = input[offset] == '\0'
		? 0
		: g_utf8_get_char_validated (input.data () + offset, input.size () - offset);
	    if (code == static_cast<gunichar> (-1) || code == static_cast<gunichar> (-2)) {
		code = 0xfffd;
		++offset;
	    } else {
		offset += code == 0 ? 1 : g_utf8_next_char (input.data () + offset) - (input.data () + offset);
	    }
	    if (code <= 0xffff) {
		utf16.push_back (code);
	    } else {
		code -= 0x10000;
		utf16.push_back (0xd800 + (code >> 10));
		utf16.push_back (0xdc00 + (code & 0x3ff));
	    }
	}
	value = JSStringCreateWithCharacters (utf16.data (), utf16.size ());
    }
    ~Str () { JSStringRelease (value); }
    operator JSStringRef () const { return value; }
};
static Holder* holder (JSValue v) { return static_cast<Holder*> (JS_VALUE_GET_PTR (v)); }
static JSValueRef toJsc (JSContext* c, JSValue v) {
    switch (JS_VALUE_GET_TAG (v)) {
	case JS_TAG_INT:
	    return JSValueMakeNumber (c->jsc, JS_VALUE_GET_INT (v));
	case JS_TAG_FLOAT64:
	    return JSValueMakeNumber (c->jsc, JS_VALUE_GET_FLOAT64 (v));
	case JS_TAG_BOOL:
	    return JSValueMakeBoolean (c->jsc, JS_VALUE_GET_BOOL (v));
	case JS_TAG_NULL:
	    return JSValueMakeNull (c->jsc);
	case JS_TAG_UNDEFINED:
	case JS_TAG_UNINITIALIZED:
	case JS_TAG_EXCEPTION:
	    return JSValueMakeUndefined (c->jsc);
	default:
	    return holder (v)->value;
    }
}
// Callback arguments are already rooted by JavaScriptCore's active call frame.
// Borrow stack handles there; JS_DupValue promotes values retained by C++.
static JSValue fromJsc (JSContext* c, JSValueRef v, Holder* borrowed = nullptr) {
    if (!v) {
	return JS_UNDEFINED;
    }
    const auto type = JSValueGetType (c->jsc, v);
    switch (type) {
	case kJSTypeUndefined:
	    return JS_UNDEFINED;
	case kJSTypeNull:
	    return JS_NULL;
	case kJSTypeBoolean:
	    return JS_NewBool (c, JSValueToBoolean (c->jsc, v));
	case kJSTypeNumber:
	    return JS_NewFloat64 (c, JSValueToNumber (c->jsc, v, nullptr));
	default:
	    break;
    }
    const int tag = type == kJSTypeString ? JS_TAG_STRING
	: type == kJSTypeSymbol           ? JS_TAG_SYMBOL
	: type == kJSTypeBigInt           ? JS_TAG_BIG_INT
					  : JS_TAG_OBJECT;
    Native* n
	= tag == JS_TAG_OBJECT ? static_cast<Native*> (JSObjectGetPrivate (const_cast<JSObjectRef> (v))) : nullptr;
    if (borrowed) {
	*borrowed = { 0, c, v, n };
	return JS_MKPTR (tag, borrowed);
    }
    auto* h = new Holder { 1, c, v, n };
    JSValueProtect (c->jsc, v);
    c->rt->holders.insert (h);
    return JS_MKPTR (tag, h);
}
static JSValue fail (JSContext* c, JSValueRef error) {
    JS_FreeValue (c, c->exception);
    c->exception = fromJsc (c, error);
    return JS_EXCEPTION;
}
static JSValue finish (JSContext* c, JSValueRef v, JSValueRef error) {
    return error ? fail (c, error) : fromJsc (c, v);
}
static void passException (JSContext* c, JSValueRef* out) {
    if (out) {
	*out = toJsc (c, c->exception);
    }
}
static JSObjectRef object (JSContext* c, JSValue v, JSValueRef* ex) {
    return JSValueToObject (c->jsc, toJsc (c, v), ex);
}
static std::string utf8 (JSStringRef str) {
    std::string s (JSStringGetMaximumUTF8CStringSize (str), '\0');
    size_t n = JSStringGetUTF8CString (str, s.data (), s.size ());
    s.resize (n ? n - 1 : 0);
    return s;
}
static JSValue nativeValue (Native* n, Holder& temporary) {
    temporary = { 1, n->ctx, nullptr, n };
    return JS_MKPTR (JS_TAG_OBJECT, &temporary);
}
static void finalize (Native* n) {
    if (n->id) {
	auto it = n->rt.load ()->classes.find (n->id);
	if (it != n->rt.load ()->classes.end () && it->second->def.finalizer) {
	    Holder h;
	    it->second->def.finalizer (n->rt, nativeValue (n, h));
	}
    }
    n->opaque = nullptr;
    for (auto v : n->data) {
	JS_FreeValue (n->ctx, v);
    }
    n->data.clear ();
    n->rt.load ()->natives.erase (n);
}
static void collected (JSObjectRef o) {
    auto* n = static_cast<Native*> (JSObjectGetPrivate (o));
    if (!n) {
	return;
    }
    auto* rt = n->rt.load ();
    if (!rt) {
	delete n;
	return;
    }
    std::lock_guard lock (rt->mutex);
    if (!n->rt.load ()) {
	delete n;
	return;
    }
    rt->collected.push_back (n);
    rt->loop.finalizersPending.store (true);
}
static void drain (JSRuntime* rt) {
    std::vector<Native*> list;
    {
	std::lock_guard lock (rt->mutex);
	list.swap (rt->collected);
    }
    for (auto* n : list) {
	finalize (n);
	delete n;
    }
}

bool WallpaperEngine::Scripting::serviceJavaScriptCoreRuntime () {
    auto* context = runtimeLoop.context.get ();
    // The last group release can leave a VM alive temporarily. Dispatching any
    // of its remaining callbacks still requires an initialized WTF main loop.
    if (!context || !runtimeLoop.dispatchWhileActive) return false;

    // Dispatch a bounded number of ready batches before returning to rendering.
    for (int i = 0; i < 4 && g_main_context_iteration (context, FALSE); ++i) { }

    // GC may queue native wrapper cleanup when a scene has no more JS calls.
    if (runtimeLoop.finalizersPending.exchange (false)) {
	for (auto* rt : runtimeLoop.runtimes) drain (rt);
    }
    return true;
}

static bool hasOrdinaryOwnProperty (Native* n, JSObjectRef obj, JSStringRef name, JSValueRef* ex) {
    // QuickJS checks ordinary own descriptors before its exotic hooks. Hide
    // those hooks during this query so JSC sees only the object's own storage.
    n->queryingOwnProperty = true;
    const JSValueRef prototype = JSObjectGetPrototype (n->ctx->jsc, obj);
    // The original Object.prototype has an immutable null prototype. A missing
    // key on these ordinary chains cannot run a user-supplied Proxy.has trap.
    if ((prototype == n->ctx->objectProto || JSValueIsNull (n->ctx->jsc, prototype))
	&& !JSObjectHasProperty (n->ctx->jsc, obj, name)) {
	n->queryingOwnProperty = false;
	return false;
    }
    const JSValueRef arg = JSValueMakeString (n->ctx->jsc, name);
    JSValueRef error = nullptr;
    const JSValueRef result = JSObjectCallAsFunction (n->ctx->jsc, n->ctx->hasOwnProperty, obj, 1, &arg, &error);
    n->queryingOwnProperty = false;
    if (error) {
	if (ex) *ex = error;
	return true;
    }
    return JSValueToBoolean (n->ctx->jsc, result);
}
static JSValueRef getNative (JSContextRef jc, JSObjectRef obj, JSStringRef name, JSValueRef* ex) {
    auto* n = static_cast<Native*> (JSObjectGetPrivate (obj));
    if (!n || !n->id || n->queryingOwnProperty) {
	return nullptr;
    }
    auto* c = n->ctx;
    auto& cls = *n->rt.load ()->classes.at (n->id);
    if (!cls.exotic.get_property) {
	return nullptr;
    }
    if (hasOrdinaryOwnProperty (n, obj, name, ex)) {
	return nullptr;
    }
    auto atom = JS_NewAtom (c, utf8 (name).c_str ());
    Holder selfHandle { 0, c, obj, n };
    JSValue self = JS_MKPTR (JS_TAG_OBJECT, &selfHandle);
    JSValue result = cls.exotic.get_property (c, self, atom, self);
    JS_FreeValue (c, self);
    if (JS_IsException (result)) {
	passException (c, ex);
	return JSValueMakeUndefined (jc);
    }
    JSValueRef value = toJsc (c, result);
    JS_FreeValue (c, result);
    return value;
}
static bool setNative (JSContextRef jc, JSObjectRef obj, JSStringRef name, JSValueRef value, JSValueRef* ex) {
    auto* n = static_cast<Native*> (JSObjectGetPrivate (obj));
    if (!n || !n->id || n->queryingOwnProperty) {
	return false;
    }
    auto* c = n->ctx;
    auto& cls = *n->rt.load ()->classes.at (n->id);
    if (!cls.exotic.set_property) {
	return false;
    }
    if (hasOrdinaryOwnProperty (n, obj, name, ex)) {
	return false;
    }
    auto atom = JS_NewAtom (c, utf8 (name).c_str ());
    Holder selfHandle { 0, c, obj, n }, valueHandle;
    auto self = JS_MKPTR (JS_TAG_OBJECT, &selfHandle), v = fromJsc (c, value, &valueHandle);
    int result = cls.exotic.set_property (c, self, atom, v, self, JS_PROP_THROW);
    JS_FreeValue (c, self);
    JS_FreeValue (c, v);
    if (result < 0) {
	passException (c, ex);
    }
    return true;
}
static JSValueRef invoke (
    JSContextRef jc, JSObjectRef fn, JSObjectRef self, size_t argc, const JSValueRef args[], JSValueRef* ex,
    bool construct
) {
    auto* n = static_cast<Native*> (JSObjectGetPrivate (fn));
    auto* c = n->ctx;
    std::vector<JSValue> argv (argc);
    std::vector<Holder> handles (argc + 1);
    for (size_t i = 0; i < argc; i++) {
	argv[i] = fromJsc (c, args[i], &handles[i]);
    }
    auto receiver = fromJsc (c, construct ? fn : self, &handles[argc]);
    JSValue result;
    if (n->dataFunction) {
	result = n->dataFunction (c, receiver, int (argc), argv.data (), n->magic, n->data.data ());
    } else {
	switch (n->kind) {
	    case JS_CFUNC_generic_magic:
	    case JS_CFUNC_constructor_magic:
	    case JS_CFUNC_constructor_or_func_magic:
		result = n->function.generic_magic (c, receiver, int (argc), argv.data (), n->magic);
		break;
	    case JS_CFUNC_getter:
		result = n->function.getter (c, receiver);
		break;
	    case JS_CFUNC_setter:
		result = n->function.setter (c, receiver, argc ? argv[0] : JS_UNDEFINED);
		break;
	    case JS_CFUNC_getter_magic:
		result = n->function.getter_magic (c, receiver, n->magic);
		break;
	    case JS_CFUNC_setter_magic:
		result = n->function.setter_magic (c, receiver, argc ? argv[0] : JS_UNDEFINED, n->magic);
		break;
	    case JS_CFUNC_f_f:
		result = JS_NewFloat64 (c, n->function.f_f (argc ? JSValueToNumber (jc, args[0], ex) : NAN));
		break;
	    case JS_CFUNC_f_f_f:
		result = JS_NewFloat64 (
		    c,
		    n->function.f_f_f (
			argc ? JSValueToNumber (jc, args[0], ex) : NAN,
			argc > 1 ? JSValueToNumber (jc, args[1], ex) : NAN
		    )
		);
		break;
	    default:
		result = n->function.generic (c, receiver, int (argc), argv.data ());
		break;
	}
    }
    for (auto v : argv) {
	JS_FreeValue (c, v);
    }
    JS_FreeValue (c, receiver);
    if (JS_IsException (result)) {
	passException (c, ex);
	return JSValueMakeUndefined (jc);
    }
    auto value = toJsc (c, result);
    JS_FreeValue (c, result);
    return value;
}
static JSValueRef
callNative (JSContextRef c, JSObjectRef fn, JSObjectRef self, size_t n, const JSValueRef a[], JSValueRef* ex) {
    return invoke (c, fn, self, n, a, ex, false);
}
static JSObjectRef constructNative (JSContextRef c, JSObjectRef fn, size_t n, const JSValueRef a[], JSValueRef* ex) {
    auto v = invoke (c, fn, fn, n, a, ex, true);
    return *ex ? nullptr : JSValueToObject (c, v, ex);
}
static bool hasInstance (JSContextRef c, JSObjectRef ctor, JSValueRef value, JSValueRef* ex) {
    if (!JSValueIsObject (c, value)) {
	return false;
    }
    Str key ("prototype");
    auto proto = JSObjectGetProperty (c, ctor, key, ex);
    if (*ex) {
	return false;
    }
    if (!JSValueIsObject (c, proto)) {
	auto* ctx = static_cast<Native*> (JSObjectGetPrivate (ctor))->ctx;
	JS_ThrowTypeError (ctx, "Constructor prototype is not an object");
	passException (ctx, ex);
	return false;
    }
    auto p = JSObjectGetPrototype (c, JSValueToObject (c, value, ex));
    while (JSValueIsObject (c, p)) {
	if (JSValueIsStrictEqual (c, p, proto)) {
	    return true;
	}
	p = JSObjectGetPrototype (c, JSValueToObject (c, p, ex));
    }
    return false;
}
static JSClassRef functionClass () {
    static auto cls = [] {
	auto d = kJSClassDefinitionEmpty;
	d.className = "NativeFunction";
	d.attributes = kJSClassAttributeNoAutomaticPrototype;
	d.finalize = collected;
	d.callAsFunction = callNative;
	d.callAsConstructor = constructNative;
	d.hasInstance = hasInstance;
	return JSClassCreate (&d);
    }();
    return cls;
}
extern "C" {
JSRuntime* JS_NewRuntime () {
    auto rt = std::make_unique<JSRuntime> (runtimeLoop);
    if (!runtimeLoop.context) runtimeLoop.context.reset (g_main_context_new ());
    runtimeLoop.runtimes.push_back (rt.get ());

    // WTF fixes its run loop's GLib context on first use on each thread.
    // Bind it before the first group and share that context across scenes.
    g_main_context_push_thread_default (runtimeLoop.context.get ());
    runtimeLoop.dispatchWhileActive = initializeMainRunLoop ();
    rt->group = JSContextGroupCreate ();
    g_main_context_pop_thread_default (runtimeLoop.context.get ());
    return rt.release ();
}
JSContext* JS_NewContext (JSRuntime* rt) {
    auto* c = new JSContext;
    c->rt = rt;
    c->jsc = JSGlobalContextCreateInGroup (rt->group, nullptr);
    rt->contexts.push_back (c);
    auto get = [&] (const char* expression) {
	Str source (expression);
	return JSValueToObject (c->jsc, JSEvaluateScript (c->jsc, source, nullptr, nullptr, 1, nullptr), nullptr);
    };
    c->defineProperty = get ("Object.defineProperty");
    c->hasOwnProperty = get ("Object.prototype.hasOwnProperty");
    c->objectProto = get ("Object.prototype");
    c->syntaxError = get ("SyntaxError");
    c->stringify = get ("JSON.stringify");
    c->functionProto = get ("Function.prototype");
    c->uint8ArrayConstructor = get ("Uint8Array");
    c->typedArrayValues = get ("Uint8Array.prototype.values");
    JSValueProtect (c->jsc, c->defineProperty);
    JSValueProtect (c->jsc, c->hasOwnProperty);
    JSValueProtect (c->jsc, c->objectProto);
    JSValueProtect (c->jsc, c->syntaxError);
    JSValueProtect (c->jsc, c->stringify);
    JSValueProtect (c->jsc, c->functionProto);
    JSValueProtect (c->jsc, c->uint8ArrayConstructor);
    JSValueProtect (c->jsc, c->typedArrayValues);
    return c;
}
void JS_FreeContext (JSContext* c) {
    for (auto& [id, v] : c->protos) {
	JS_FreeValue (c, v);
    }
    c->protos.clear ();
    for (auto& m : c->modules) {
	for (auto& [k, v] : m->exports) {
	    JS_FreeValue (c, v);
	}
    }
    c->modules.clear ();
    JS_FreeValue (c, c->exception);
    c->exception = JS_UNINITIALIZED;
    drain (c->rt);
}
void JS_FreeRuntime (JSRuntime* rt) {
    drain (rt);
    const std::vector<Native*> natives (rt->natives.begin (), rt->natives.end ());
    // Releasing captured values enters JavaScriptCore. Its GC callbacks can
    // take our queue lock, so never hold that lock while releasing values.
    for (auto* n : natives) {
	finalize (n);
    }
    {
	std::lock_guard lock (rt->mutex);
	for (auto* n : natives) {
	    n->rt = nullptr;
	}
	// These callbacks already ran before detachment, so they will not run
	// again when the context is released below.
	for (auto* n : rt->collected) {
	    delete n;
	}
	rt->collected.clear ();
    }
    while (!rt->holders.empty ()) {
	auto* h = *rt->holders.begin ();
	rt->holders.erase (h);
	JSValueUnprotect (h->ctx->jsc, h->value);
	delete h;
    }
    for (auto* c : rt->contexts) {
	JSValueUnprotect (c->jsc, c->defineProperty);
	JSValueUnprotect (c->jsc, c->hasOwnProperty);
	JSValueUnprotect (c->jsc, c->objectProto);
	JSValueUnprotect (c->jsc, c->syntaxError);
	JSValueUnprotect (c->jsc, c->stringify);
	JSValueUnprotect (c->jsc, c->functionProto);
	JSValueUnprotect (c->jsc, c->uint8ArrayConstructor);
	JSValueUnprotect (c->jsc, c->typedArrayValues);
	JSGlobalContextRelease (c->jsc);
    }
    JSContextGroupRelease (rt->group);
    for (auto& [id, c] : rt->classes) {
	JSClassRelease (c->klass);
    }
    for (auto* c : rt->contexts) {
	delete c;
    }
    std::erase (rt->loop.runtimes, rt);
    delete rt;
}
JSValue JS_DupValue (JSContext*, JSValue v) {
    if (JS_VALUE_HAS_REF_COUNT (v)) {
	auto* h = holder (v);
	if (h->refs == 0) {
	    return fromJsc (h->ctx, h->value);
	}
	++h->refs;
    }
    return v;
}
void JS_FreeValue (JSContext*, JSValue v) {
    if (JS_VALUE_HAS_REF_COUNT (v)) {
	auto* h = holder (v);
	if (h->refs == 0) {
	    return;
	}
	if (--h->refs == 0) {
	    h->ctx->rt->holders.erase (h);
	    JSValueUnprotect (h->ctx->jsc, h->value);
	    delete h;
	}
    }
}
void JS_FreeValueRT (JSRuntime*, JSValue v) { JS_FreeValue (nullptr, v); }
void JS_MarkValue (JSRuntime*, JSValue, JS_MarkFunc*) { }
void JS_SetDumpFlags (JSRuntime*, uint64_t) { }
void js_free (JSContext*, void* p) { free (p); }
JSValue JS_GetException (JSContext* c) {
    auto v = c->exception;
    c->exception = JS_UNINITIALIZED;
    return v;
}
JSValue JS_GetGlobalObject (JSContext* c) { return fromJsc (c, JSContextGetGlobalObject (c->jsc)); }
JSValue JS_NewObject (JSContext* c) { return fromJsc (c, JSObjectMake (c->jsc, nullptr, nullptr)); }
JSValue JS_NewArray (JSContext* c) {
    JSValueRef ex = nullptr;
    auto v = JSObjectMakeArray (c->jsc, 0, nullptr, &ex);
    return finish (c, v, ex);
}
bool JS_IsArray (JSValue obj) {
    if (!JS_IsObject (obj)) return false;
    const auto* value = holder (obj);
    return JSValueIsArray (value->ctx->jsc, value->value);
}
JSClassID JS_NewClassID (JSRuntime* rt, JSClassID* id) { return *id = rt->nextClass++; }
int JS_NewClass (JSRuntime* rt, JSClassID id, const JSClassDef* def) {
    auto cls = std::make_unique<Class> ();
    cls->def = *def;
    if (def->exotic) {
	cls->exotic = *def->exotic;
    }
    auto d = kJSClassDefinitionEmpty;
    d.className = def->class_name;
    d.attributes = kJSClassAttributeNoAutomaticPrototype;
    d.finalize = collected;
    d.getProperty = getNative;
    d.setProperty = setNative;
    cls->klass = JSClassCreate (&d);
    rt->classes[id] = std::move (cls);
    return 0;
}
JSValue JS_NewObjectClass (JSContext* c, JSClassID id) {
    auto* n = new Native;
    n->ctx = c;
    n->rt = c->rt;
    n->id = id;
    c->rt->natives.insert (n);
    auto o = JSObjectMake (c->jsc, c->rt->classes.at (id)->klass, n);
    auto it = c->protos.find (id);
    if (it != c->protos.end ()) {
	JSObjectSetPrototype (c->jsc, o, toJsc (c, it->second));
    }
    return fromJsc (c, o);
}
void JS_SetClassProto (JSContext* c, JSClassID id, JSValue v) {
    auto it = c->protos.find (id);
    if (it != c->protos.end ()) {
	JS_FreeValue (c, it->second);
    }
    c->protos[id] = v;
}
JSClassID JS_GetClassID (JSValue v) { return JS_IsObject (v) && holder (v)->native ? holder (v)->native->id : 0; }
int JS_SetOpaque (JSValue v, void* p) {
    if (JS_IsObject (v) && holder (v)->native) {
	holder (v)->native->opaque = p;
	return 0;
    }
    return -1;
}
void* JS_GetOpaque (JSValue v, JSClassID id) {
    return JS_GetClassID (v) == id && id ? holder (v)->native->opaque : nullptr;
}
void* JS_GetAnyOpaque (JSValue v, JSClassID* id) {
    *id = JS_GetClassID (v);
    return *id ? holder (v)->native->opaque : nullptr;
}
JSAtom JS_NewAtom (JSContext* c, const char* s) {
    auto [it, added] = c->atomMap.emplace (s, c->atoms.size ());
    if (added) {
	c->atoms.emplace_back (s);
    }
    return it->second;
}
void JS_FreeAtom (JSContext*, JSAtom) { }
const char* JS_AtomToCStringLen (JSContext* c, size_t* len, JSAtom atom) {
    const auto& s = c->atoms.at (atom);
    if (len) {
	*len = s.size ();
    }
    return strdup (s.c_str ());
}
JSValue JS_NewStringLen (JSContext* c, const char* s, size_t len) {
    Str str (std::string_view (s, len));
    return fromJsc (c, JSValueMakeString (c->jsc, str));
}
const char* JS_ToCStringLen2 (JSContext* c, size_t* len, JSValue v, bool) {
    JSValueRef ex = nullptr;
    auto str = JSValueToStringCopy (c->jsc, toJsc (c, v), &ex);
    if (ex) {
	fail (c, ex);
	return nullptr;
    }
    auto s = utf8 (str);
    JSStringRelease (str);
    if (len) {
	*len = s.size ();
    }
    char* bytes = static_cast<char*> (malloc (s.size () + 1));
    memcpy (bytes, s.data (), s.size ());
    bytes[s.size ()] = '\0';
    return bytes;
}
void JS_FreeCString (JSContext*, const char* p) { free (const_cast<char*> (p)); }
JSValue JS_GetPropertyStr (JSContext* c, JSValue v, const char* key) {
    JSValueRef ex = nullptr;
    auto o = object (c, v, &ex);
    if (ex) {
	return fail (c, ex);
    }
    Str str (key);
    auto result = JSObjectGetProperty (c->jsc, o, str, &ex);
    return finish (c, result, ex);
}
JSValue JS_GetProperty (JSContext* c, JSValue v, JSAtom a) {
    return JS_GetPropertyStr (c, v, c->atoms.at (a).c_str ());
}
JSValue JS_GetPropertyUint32 (JSContext* c, JSValue v, uint32_t index) {
    JSValueRef ex = nullptr;
    auto o = object (c, v, &ex);
    if (ex) {
	return fail (c, ex);
    }
    auto r = JSObjectGetPropertyAtIndex (c->jsc, o, index, &ex);
    return finish (c, r, ex);
}
int JS_SetPropertyStr (JSContext* c, JSValue obj, const char* key, JSValue v) {
    JSValueRef ex = nullptr;
    auto o = object (c, obj, &ex);
    if (!ex) {
	Str str (key);
	JSObjectSetProperty (c->jsc, o, str, toJsc (c, v), kJSPropertyAttributeNone, &ex);
    }
    JS_FreeValue (c, v);
    if (ex) {
	fail (c, ex);
	return -1;
    }
    return 1;
}
int JS_SetPropertyUint32 (JSContext* c, JSValue obj, uint32_t i, JSValue v) {
    return JS_SetPropertyStr (c, obj, std::to_string (i).c_str (), v);
}
JSValue JS_GetPrototype (JSContext* c, JSValue obj) {
    JSValueRef ex = nullptr;
    auto o = object (c, obj, &ex);
    if (ex) {
	return fail (c, ex);
    }
    return fromJsc (c, JSObjectGetPrototype (c->jsc, o));
}
static int define (JSContext* c, JSValue obj, JSStringRef key, JSObjectRef desc) {
    JSValueRef ex = nullptr;
    JSValueRef args[] = { toJsc (c, obj), JSValueMakeString (c->jsc, key), desc };
    JSObjectCallAsFunction (c->jsc, c->defineProperty, nullptr, 3, args, &ex);
    if (ex) {
	fail (c, ex);
	return -1;
    }
    return 1;
}
static void field (JSContext* c, JSObjectRef obj, const char* key, JSValueRef v) {
    Str s (key);
    JSObjectSetProperty (c->jsc, obj, s, v, kJSPropertyAttributeNone, nullptr);
}
int JS_DefinePropertyValueStr (JSContext* c, JSValue obj, const char* key, JSValue v, int flags) {
    auto d = JSObjectMake (c->jsc, nullptr, nullptr);
    field (c, d, "value", toJsc (c, v));
    field (c, d, "writable", JSValueMakeBoolean (c->jsc, flags & JS_PROP_WRITABLE));
    field (c, d, "enumerable", JSValueMakeBoolean (c->jsc, flags & JS_PROP_ENUMERABLE));
    field (c, d, "configurable", JSValueMakeBoolean (c->jsc, flags & JS_PROP_CONFIGURABLE));
    Str s (key);
    int r = define (c, obj, s, d);
    JS_FreeValue (c, v);
    return r;
}
int JS_DefinePropertyGetSet (JSContext* c, JSValue obj, JSAtom atom, JSValue getter, JSValue setter, int flags) {
    auto d = JSObjectMake (c->jsc, nullptr, nullptr);
    field (c, d, "get", toJsc (c, getter));
    field (c, d, "set", toJsc (c, setter));
    field (c, d, "enumerable", JSValueMakeBoolean (c->jsc, flags & JS_PROP_ENUMERABLE));
    field (c, d, "configurable", JSValueMakeBoolean (c->jsc, flags & JS_PROP_CONFIGURABLE));
    Str s (c->atoms.at (atom));
    int r = define (c, obj, s, d);
    JS_FreeValue (c, getter);
    JS_FreeValue (c, setter);
    return r;
}
int JS_ToBool (JSContext* c, JSValue v) { return JSValueToBoolean (c->jsc, toJsc (c, v)); }
int JS_ToFloat64 (JSContext* c, double* out, JSValue v) {
    JSValueRef ex = nullptr;
    *out = JSValueToNumber (c->jsc, toJsc (c, v), &ex);
    if (ex) {
	fail (c, ex);
	return -1;
    }
    return 0;
}
int JS_ToInt32 (JSContext* c, int32_t* out, JSValue v) {
    double d;
    if (JS_ToFloat64 (c, &d, v) < 0) {
	return -1;
    }
    double n = std::isfinite (d) ? std::fmod (std::trunc (d), 4294967296.0) : 0;
    if (n < 0) {
	n += 4294967296.0;
    }
    *out = static_cast<int32_t> (static_cast<uint32_t> (n));
    return 0;
}
int JS_ToInt64 (JSContext* c, int64_t* out, JSValue v) {
    double d;
    if (JS_ToFloat64 (c, &d, v) < 0) {
	return -1;
    }
    *out = std::isnan (d)         ? 0
	: d >= double (INT64_MAX) ? INT64_MAX
	: d <= double (INT64_MIN) ? INT64_MIN
				  : static_cast<int64_t> (d);
    return 0;
}
bool JS_IsFunction (JSContext* c, JSValue v) {
    return JS_IsObject (v) && JSObjectIsFunction (c->jsc, JSValueToObject (c->jsc, toJsc (c, v), nullptr));
}
int JS_IsEqual (JSContext* c, JSValue a, JSValue b) {
    JSValueRef ex = nullptr;
    bool r = JSValueIsEqual (c->jsc, toJsc (c, a), toJsc (c, b), &ex);
    if (ex) {
	fail (c, ex);
	return -1;
    }
    return r;
}
JSValue JS_Call (JSContext* c, JSValue fn, JSValue self, int argc, JSValue* argv) {
    JSValueRef ex = nullptr;
    auto f = object (c, fn, &ex);
    if (ex) {
	return fail (c, ex);
    }
    std::vector<JSValueRef> args;
    for (int i = 0; i < argc; i++) {
	args.push_back (toJsc (c, argv[i]));
    }
    auto thisObj = JS_IsNull (self) || JS_IsUndefined (self) ? nullptr : object (c, self, &ex);
    if (ex) {
	return fail (c, ex);
    }
    auto r = JSObjectCallAsFunction (c->jsc, f, thisObj, argc, args.data (), &ex);
    auto result = finish (c, r, ex);
    drain (c->rt);
    return result;
}
JSValue JS_CallConstructor (JSContext* c, JSValue fn, int argc, JSValue* argv) {
    JSValueRef ex = nullptr;
    auto f = object (c, fn, &ex);
    if (ex) {
	return fail (c, ex);
    }
    std::vector<JSValueRef> args;
    for (int i = 0; i < argc; i++) {
	args.push_back (toJsc (c, argv[i]));
    }
    auto r = JSObjectCallAsConstructor (c->jsc, f, argc, args.data (), &ex);
    auto result = finish (c, r, ex);
    drain (c->rt);
    return result;
}
JSValue JS_Eval (JSContext* c, const char* text, size_t size, const char* filename, int flags) {
    if ((flags & JS_EVAL_TYPE_MASK) != JS_EVAL_TYPE_GLOBAL) {
	return JS_ThrowInternalError (c, "SceneScript source must be lowered before evaluation");
    }
    Str src (std::string (text, size)), name (filename);
    JSValueRef ex = nullptr;
    auto r = JSEvaluateScript (c->jsc, src, nullptr, name, 1, &ex);
    auto result = finish (c, r, ex);
    drain (c->rt);
    return result;
}
int JS_IsInstanceOf (JSContext* c, JSValue value, JSValue ctor) {
    JSValueRef ex = nullptr;
    auto o = object (c, ctor, &ex);
    if (ex) {
	fail (c, ex);
	return -1;
    }
    bool result = JSValueIsInstanceOfConstructor (c->jsc, toJsc (c, value), o, &ex);
    if (ex) {
	fail (c, ex);
	return -1;
    }
    return result;
}
JSValue JS_GetModuleNamespace (JSContext* c, JSModuleDef* m) {
    if (!m->initialized) {
	if (m->init (c, m) < 0) {
	    return JS_EXCEPTION;
	}
	m->initialized = true;
    }
    auto ns = JS_NewObject (c);
    for (auto& [name, v] : m->exports) {
	JS_DefinePropertyValueStr (c, ns, name.c_str (), JS_DupValue (c, v), JS_PROP_ENUMERABLE);
    }
    return ns;
}
static JSValue makeFunction (JSContext* c, Native* n, const char* name, int length) {
    c->rt->natives.insert (n);
    auto fn = JSObjectMake (c->jsc, functionClass (), n);
    auto value = fromJsc (c, fn);
    // These are new own data properties. Define them directly instead of
    // building descriptors and calling back into JavaScript for every method.
    static const Str nameKey ("name"), lengthKey ("length");
    Str functionName (name);
    constexpr auto attributes = kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum;
    JSObjectSetProperty (c->jsc, fn, nameKey, JSValueMakeString (c->jsc, functionName), attributes, nullptr);
    JSObjectSetProperty (c->jsc, fn, lengthKey, JSValueMakeNumber (c->jsc, length), attributes, nullptr);
    JSObjectSetPrototype (c->jsc, fn, c->functionProto);
    return value;
}
JSValue
JS_NewCFunction2 (JSContext* c, JSCFunction* fn, const char* name, int length, JSCFunctionEnum kind, int magic) {
    auto* n = new Native;
    n->ctx = c;
    n->rt = c->rt;
    n->function.generic = fn;
    n->kind = kind;
    n->magic = magic;
    return makeFunction (c, n, name, length);
}
JSValue JS_NewCFunctionData (JSContext* c, JSCFunctionData* fn, int length, int magic, int count, JSValue* data) {
    auto* n = new Native;
    n->ctx = c;
    n->rt = c->rt;
    n->dataFunction = fn;
    n->magic = magic;
    for (int i = 0; i < count; i++) {
	n->data.push_back (JS_DupValue (c, data[i]));
    }
    return makeFunction (c, n, "", length);
}
void JS_SetConstructor (JSContext* c, JSValue ctor, JSValue proto) {
    JS_DefinePropertyValueStr (c, ctor, "prototype", JS_DupValue (c, proto), JS_PROP_WRITABLE);
    JS_DefinePropertyValueStr (c, proto, "constructor", JS_DupValue (c, ctor), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
}
JSModuleDef* JS_NewCModule (JSContext* c, const char* name, JSModuleInitFunc* init) {
    auto m = std::make_unique<JSModuleDef> ();
    m->name = name;
    m->init = init;
    auto* p = m.get ();
    c->modules.push_back (std::move (m));
    return p;
}
int JS_AddModuleExport (JSContext*, JSModuleDef*, const char*) { return 0; }
int JS_SetModuleExport (JSContext*, JSModuleDef* m, const char* key, JSValue v) {
    m->exports[key] = v;
    return 0;
}
void JS_SetModuleLoaderFunc (JSRuntime*, JSModuleNormalizeFunc*, JSModuleLoaderFunc*, void*) { }
JSValue JS_ParseJSON (JSContext* c, const char* data, size_t size, const char*) {
    Str str (std::string (data, size));
    auto r = JSValueMakeFromJSONString (c->jsc, str);
    return r ? fromJsc (c, r) : JS_ThrowTypeError (c, "Invalid JSON");
}
JSValue JS_JSONStringify (JSContext* c, JSValue obj, JSValue replacer, JSValue space) {
    JSValueRef ex = nullptr;
    JSValueRef args[] = { toJsc (c, obj), toJsc (c, replacer), toJsc (c, space) };
    auto r = JSObjectCallAsFunction (c->jsc, c->stringify, nullptr, 3, args, &ex);
    return finish (c, r, ex);
}
int JS_GetOwnPropertyNames (JSContext* c, JSPropertyEnum** props, uint32_t* len, JSValue obj, int flags) {
    JSValueRef ex = nullptr;
    auto o = object (c, obj, &ex);
    if (ex) {
	fail (c, ex);
	return -1;
    }
    auto names = JSObjectCopyPropertyNames (c->jsc, o);
    *len = JSPropertyNameArrayGetCount (names);
    *props = static_cast<JSPropertyEnum*> (calloc (*len, sizeof (JSPropertyEnum)));
    for (uint32_t i = 0; i < *len; i++) {
	(*props)[i].is_enumerable = true;
	(*props)[i].atom = JS_NewAtom (c, utf8 (JSPropertyNameArrayGetNameAtIndex (names, i)).c_str ());
    }
    JSPropertyNameArrayRelease (names);
    return 0;
}
struct Buffer {
    JSRuntime* rt;
    JSFreeArrayBufferDataFunc* free;
    void* opaque;
};
static void freeBuffer (void* bytes, void* data) {
    auto* b = static_cast<Buffer*> (data);
    if (b->free) {
	b->free (b->rt, b->opaque, bytes);
    }
    delete b;
}
JSValue JS_NewArrayBuffer (
    JSContext* c, uint8_t* bytes, size_t size, JSFreeArrayBufferDataFunc* fn, void* opaque, bool shared
) {
    auto* b = new Buffer { c->rt, fn, opaque };
    JSValueRef ex = nullptr;
    auto r = JSObjectMakeArrayBufferWithBytesNoCopy (c->jsc, bytes, size, freeBuffer, b, &ex);
    return finish (c, r, ex);
}
uint8_t* JS_GetArrayBuffer (JSContext* c, size_t* size, JSValue obj) {
    *size = 0;
    JSValueRef ex = nullptr;
    auto value = toJsc (c, obj);
    if (JSValueGetTypedArrayType (c->jsc, value, &ex) != kJSTypedArrayTypeArrayBuffer) {
	if (ex) fail (c, ex);
	else JS_ThrowTypeError (c, "not an ArrayBuffer");
	return nullptr;
    }
    // The C API's byte getters do not reject detached buffers. A zero-length
    // native view validates attachment without reading script-owned properties.
    JSValueRef args[] = { value, JSValueMakeNumber (c->jsc, 0), JSValueMakeNumber (c->jsc, 0) };
    JSObjectCallAsConstructor (c->jsc, c->uint8ArrayConstructor, 3, args, &ex);
    if (ex) {
	fail (c, ex);
	return nullptr;
    }
    auto buffer = const_cast<JSObjectRef> (value);
    auto length = JSObjectGetArrayBufferByteLength (c->jsc, buffer, &ex);
    auto bytes = JSObjectGetArrayBufferBytesPtr (c->jsc, buffer, &ex);
    if (ex) {
	fail (c, ex);
	return nullptr;
    }
    *size = length;
    return static_cast<uint8_t*> (bytes);
}
int JS_GetTypedArrayType (JSValue obj) {
    if (!JS_IsObject (obj)) return -1;
    const auto* value = holder (obj);
    switch (JSValueGetTypedArrayType (value->ctx->jsc, value->value, nullptr)) {
	case kJSTypedArrayTypeUint8ClampedArray: return JS_TYPED_ARRAY_UINT8C;
	case kJSTypedArrayTypeInt8Array: return JS_TYPED_ARRAY_INT8;
	case kJSTypedArrayTypeUint8Array: return JS_TYPED_ARRAY_UINT8;
	case kJSTypedArrayTypeInt16Array: return JS_TYPED_ARRAY_INT16;
	case kJSTypedArrayTypeUint16Array: return JS_TYPED_ARRAY_UINT16;
	case kJSTypedArrayTypeInt32Array: return JS_TYPED_ARRAY_INT32;
	case kJSTypedArrayTypeUint32Array: return JS_TYPED_ARRAY_UINT32;
	case kJSTypedArrayTypeBigInt64Array: return JS_TYPED_ARRAY_BIG_INT64;
	case kJSTypedArrayTypeBigUint64Array: return JS_TYPED_ARRAY_BIG_UINT64;
	case kJSTypedArrayTypeFloat32Array: return JS_TYPED_ARRAY_FLOAT32;
	case kJSTypedArrayTypeFloat64Array: return JS_TYPED_ARRAY_FLOAT64;
	default: return -1;
    }
}
JSValue JS_GetTypedArrayBuffer (
    JSContext* c, JSValue obj, size_t* byteOffset, size_t* byteLength, size_t* bytesPerElement
) {
    JSValueRef ex = nullptr;
    auto value = toJsc (c, obj);
    const auto type = JSValueGetTypedArrayType (c->jsc, value, &ex);
    if (ex) return fail (c, ex);
    size_t width;
    switch (type) {
	case kJSTypedArrayTypeInt8Array:
	case kJSTypedArrayTypeUint8Array:
	case kJSTypedArrayTypeUint8ClampedArray: width = 1; break;
	case kJSTypedArrayTypeInt16Array:
	case kJSTypedArrayTypeUint16Array: width = 2; break;
	case kJSTypedArrayTypeInt32Array:
	case kJSTypedArrayTypeUint32Array:
	case kJSTypedArrayTypeFloat32Array: width = 4; break;
	case kJSTypedArrayTypeFloat64Array:
	case kJSTypedArrayTypeBigInt64Array:
	case kJSTypedArrayTypeBigUint64Array: width = 8; break;
	default: return JS_ThrowTypeError (c, "not a TypedArray");
    }
    auto view = const_cast<JSObjectRef> (value);
    // The intrinsic validates detached and out-of-bounds resizable views; the
    // C metadata getters return zero for those as well as valid empty views.
    JSObjectCallAsFunction (c->jsc, c->typedArrayValues, view, 0, nullptr, &ex);
    if (ex) return fail (c, ex);
    auto buffer = JSObjectGetTypedArrayBuffer (c->jsc, view, &ex);
    if (ex) return fail (c, ex);
    const auto offset = JSObjectGetTypedArrayByteOffset (c->jsc, view, &ex);
    const auto length = JSObjectGetTypedArrayByteLength (c->jsc, view, &ex);
    if (ex) return fail (c, ex);
    if (byteOffset) *byteOffset = offset;
    if (byteLength) *byteLength = length;
    if (bytesPerElement) *bytesPerElement = width;
    return fromJsc (c, buffer);
}
static JSValue throwError (JSContext* c, const char* type, const char* fmt, va_list args) {
    char* message = nullptr;
    vasprintf (&message, fmt, args);
    Str key (type), msg (message ? message : "");
    free (message);
    auto ctor = std::strcmp (type, "SyntaxError") == 0 ? c->syntaxError : JSValueToObject (
	c->jsc, JSObjectGetProperty (c->jsc, JSContextGetGlobalObject (c->jsc), key, nullptr), nullptr
    );
    auto arg = JSValueMakeString (c->jsc, msg);
    auto error = JSObjectCallAsConstructor (c->jsc, ctor, 1, &arg, nullptr);
    return fail (c, error);
}
#define THROW(NAME, TYPE)                                                                                              \
    JSValue NAME (JSContext* c, const char* fmt, ...) {                                                                \
	va_list args;                                                                                                  \
	va_start (args, fmt);                                                                                          \
	auto r = throwError (c, TYPE, fmt, args);                                                                      \
	va_end (args);                                                                                                 \
	return r;                                                                                                      \
    }
THROW(JS_ThrowTypeError,"TypeError") THROW(JS_ThrowRangeError,"RangeError") THROW(JS_ThrowReferenceError,"ReferenceError") THROW(JS_ThrowInternalError,"Error")
THROW(JS_ThrowSyntaxError,"SyntaxError")
}
