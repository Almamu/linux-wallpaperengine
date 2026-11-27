#include "quickjs.h"
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <string>
#ifdef WPENGINE_JAVASCRIPTCORE
#include "WallpaperEngine/Scripting/JavaScriptCoreRuntime.h"
#include <filesystem>
#include <glib.h>
#include <memory>
#endif

namespace {
struct Runtime {
    JSRuntime* runtime = JS_NewRuntime ();
    JSContext* context = JS_NewContext (runtime);
    ~Runtime () {
	JS_FreeContext (context);
	JS_FreeRuntime (runtime);
    }
};
JSValue captured (JSContext* context, JSValueConst, int, JSValueConst*, int, JSValue* data) {
    return JS_DupValue (context, data[0]);
}
JSValue retainArgument (JSContext* context, JSValueConst, int argc, JSValueConst* args) {
    return argc ? JS_NewCFunctionData (context, captured, 0, 0, 1, args) : JS_UNDEFINED;
}
void finalized (JSRuntime*, JSValueConst value) {
    JSClassID id;
    auto* count = static_cast<int*> (JS_GetAnyOpaque (value, &id));
    if (count) {
	++*count;
    }
}
}

TEST_CASE ("Script runtime strings preserve embedded nulls and Unicode", "[script-runtime]") {
    Runtime vm;
    const std::string input ("before\0after \xF0\x9F\x8C\x8A", 17);
    JSValue value = JS_NewStringLen (vm.context, input.data (), input.size ());
    REQUIRE_FALSE (JS_IsException (value));
    size_t length = 0;
    const char* bytes = JS_ToCStringLen (vm.context, &length, value);
    REQUIRE (bytes != nullptr);
    CHECK (length == input.size ());
    CHECK (std::string (bytes, length) == input);
    JS_FreeCString (vm.context, bytes);
    JS_FreeValue (vm.context, value);
}

TEST_CASE ("Script runtime callbacks retain captured objects and release native wrappers", "[script-runtime]") {
    int finalizers = 0;
    for (int iteration = 0; iteration < 20; ++iteration) {
	Runtime vm;
	JSClassID id = 0;
	JS_NewClassID (vm.runtime, &id);
	const JSClassDef definition { .class_name = "LifetimeProbe", .finalizer = finalized };
	REQUIRE (JS_NewClass (vm.runtime, id, &definition) == 0);
	JSValue object = JS_NewObjectClass (vm.context, id);
	JS_SetOpaque (object, &finalizers);
	JSValue fn = JS_NewCFunctionData (vm.context, captured, 0, 0, 1, &object);
	JS_FreeValue (vm.context, object);
	JSValue result = JS_Call (vm.context, fn, JS_UNDEFINED, 0, nullptr);
	REQUIRE_FALSE (JS_IsException (result));
	CHECK (JS_GetOpaque (result, id) == &finalizers);
	JS_FreeValue (vm.context, result);
	JS_FreeValue (vm.context, fn);
    }
    CHECK (finalizers == 20);
}

TEST_CASE ("Script runtime promotes callback arguments retained beyond their call", "[script-runtime]") {
    Runtime vm;
    JSValue global = JS_GetGlobalObject (vm.context);
    JS_SetPropertyStr (vm.context, global, "retain", JS_NewCFunction (vm.context, retainArgument, "retain", 1));
    const char* source = "const later = retain({value: 17}); later().value";
    JSValue result = JS_Eval (vm.context, source, strlen (source), "retain.js", JS_EVAL_TYPE_GLOBAL);
    REQUIRE_FALSE (JS_IsException (result));
    int32_t number = 0;
    REQUIRE (JS_ToInt32 (vm.context, &number, result) == 0);
    CHECK (number == 17);
    JS_FreeValue (vm.context, result);
    JS_FreeValue (vm.context, global);
}

TEST_CASE ("Script runtime native functions preserve property descriptors", "[script-runtime]") {
    Runtime vm;
    JSValue global = JS_GetGlobalObject (vm.context);
    JS_SetPropertyStr (vm.context, global, "retain", JS_NewCFunction (vm.context, retainArgument, "retain", 1));
    const char* source = R"JS(
        const name = Object.getOwnPropertyDescriptor(retain, 'name');
        const length = Object.getOwnPropertyDescriptor(retain, 'length');
        name.value === 'retain' && length.value === 1 &&
        !name.writable && !name.enumerable && name.configurable &&
        !length.writable && !length.enumerable && length.configurable &&
        Object.getPrototypeOf(retain) === Function.prototype
    )JS";
    JSValue result = JS_Eval (vm.context, source, strlen (source), "function.js", JS_EVAL_TYPE_GLOBAL);
    REQUIRE_FALSE (JS_IsException (result));
    CHECK (JS_ToBool (vm.context, result) == 1);
    JS_FreeValue (vm.context, result);
    JS_FreeValue (vm.context, global);
}

#ifdef WPENGINE_JAVASCRIPTCORE
namespace {
size_t openTimerDescriptors () {
    size_t count = 0;
    for (const auto& entry : std::filesystem::directory_iterator ("/proc/self/fd")) {
	std::error_code error;
	if (std::filesystem::read_symlink (entry.path (), error) == "anon_inode:[timerfd]") ++count;
    }
    return count;
}

}

TEST_CASE ("Script runtime services retired VMs without dispatching the default GLib context", "[script-runtime]") {
    using WallpaperEngine::Scripting::serviceJavaScriptCoreRuntime;
    bool defaultSourceDispatched = false;
    const auto destroySource = [] (GSource* source) {
	g_source_destroy (source);
	g_source_unref (source);
    };
    const std::unique_ptr<GSource, decltype(destroySource)> defaultSource (g_idle_source_new (), destroySource);
    g_source_set_callback (defaultSource.get (), [] (gpointer data) -> gboolean {
	*static_cast<bool*> (data) = true;
	return G_SOURCE_REMOVE;
    }, &defaultSourceDispatched, nullptr);
    g_source_attach (defaultSource.get (), g_main_context_default ());

    serviceJavaScriptCoreRuntime ();
    { Runtime warmup; }
    if (!serviceJavaScriptCoreRuntime ()) SKIP ("JavaScriptCore main-loop bootstrap is unavailable");
    const auto baseline = openTimerDescriptors ();

    for (int iteration = 0; iteration < 64; ++iteration) {
	{ Runtime vm; }
	serviceJavaScriptCoreRuntime ();
    }

    CHECK (openTimerDescriptors () == baseline);
    CHECK_FALSE (defaultSourceDispatched);
}

#endif

TEST_CASE ("Script runtime releases many captured callbacks at shutdown", "[script-runtime]") {
    for (int iteration = 0; iteration < 3; ++iteration) {
	Runtime vm;
	JSValue global = JS_GetGlobalObject (vm.context);
	JS_SetPropertyStr (vm.context, global, "retain", JS_NewCFunction (vm.context, retainArgument, "retain", 1));
	const char* source = "for (let i = 0; i < 20000; ++i) retain({value: i}); 42";
	JSValue result = JS_Eval (vm.context, source, strlen (source), "cleanup.js", JS_EVAL_TYPE_GLOBAL);
	REQUIRE_FALSE (JS_IsException (result));
	int32_t number = 0;
	REQUIRE (JS_ToInt32 (vm.context, &number, result) == 0);
	CHECK (number == 42);
	JS_FreeValue (vm.context, result);
	JS_FreeValue (vm.context, global);
    }
}

namespace {
struct DescriptorState {
    int known = 42;
    int writes = 0;
};
JSValue descriptorGet (JSContext* context, JSValueConst object, JSAtom atom, JSValueConst) {
    JSClassID id;
    auto* state = static_cast<DescriptorState*> (JS_GetAnyOpaque (object, &id));
    const char* name = JS_AtomToCString (context, atom);
    const bool known = name != nullptr && std::strcmp (name, "known") == 0;
    JS_FreeCString (context, name);
    return known ? JS_NewInt32 (context, state->known) : JS_UNDEFINED;
}
int descriptorSet (JSContext* context, JSValueConst object, JSAtom atom, JSValueConst value, JSValueConst, int) {
    JSClassID id;
    auto* state = static_cast<DescriptorState*> (JS_GetAnyOpaque (object, &id));
    const char* name = JS_AtomToCString (context, atom);
    const bool known = name != nullptr && std::strcmp (name, "known") == 0;
    JS_FreeCString (context, name);
    if (known) {
	++state->writes;
	return JS_ToInt32 (context, &state->known, value) < 0 ? -1 : 1;
    }
    // The layer adapter deliberately ignores an undeclared direct assignment.
    return 1;
}
}

TEST_CASE ("Script runtime own descriptors precede exotic layer-like properties", "[script-runtime]") {
    DescriptorState state;
    Runtime vm;
    JSClassID id = 0;
    JS_NewClassID (vm.runtime, &id);
    JSClassExoticMethods exotic { .get_property = descriptorGet, .set_property = descriptorSet };
    const JSClassDef definition { .class_name = "DescriptorProbe", .exotic = &exotic };
    REQUIRE (JS_NewClass (vm.runtime, id, &definition) == 0);
    JSValue object = JS_NewObjectClass (vm.context, id);
    JS_SetOpaque (object, &state);
    JSValue global = JS_GetGlobalObject (vm.context);
    REQUIRE (JS_SetPropertyStr (vm.context, global, "layer", object) == 1);
    JS_FreeValue (vm.context, global);

    const char* source = nullptr;
    SECTION ("Own data descriptors support nested mutation, replacement, and write protection") {
	source = R"JS((function () {
            Object.defineProperty(layer, 'extensionScale', {
                value: {x: 320, y: 128}, writable: true, enumerable: true, configurable: true});
            if (layer.extensionScale.x !== 320 || layer.extensionScale.y !== 128) return false;
            layer.extensionScale.x = 512;
            if (layer.extensionScale.x !== 512) return false;
            const replacement = {x: 17, y: 18};
            layer.extensionScale = replacement;
            const d = Object.getOwnPropertyDescriptor(layer, 'extensionScale');
            if (layer.extensionScale !== replacement || d.value !== replacement ||
                !d.writable || !d.enumerable || !d.configurable) return false;
            Object.defineProperty(layer, 'extensionScale', {writable: false});
            let threw = false;
            try { (function () {'use strict'; layer.extensionScale = 0;})(); }
            catch (error) { threw = error instanceof TypeError; }
            if (!threw || layer.extensionScale !== replacement) return false;
            return delete layer.extensionScale && layer.extensionScale === undefined;
        })())JS";
    }
    SECTION ("Own accessor callbacks and their exceptions survive native dispatch") {
	source = R"JS((function () {
            let gets = 0, sets = 0, stored = 12;
            Object.defineProperty(layer, 'custom', {enumerable: true, configurable: true,
                get() { ++gets; return stored; }, set(value) { ++sets; stored = value; }});
            if (layer.custom !== 12) return false;
            layer.custom = 23;
            if (layer.custom !== 23 || gets !== 2 || sets !== 1) return false;
            const d = Object.getOwnPropertyDescriptor(layer, 'custom');
            if (typeof d.get !== 'function' || typeof d.set !== 'function' ||
                !d.enumerable || !d.configurable || 'value' in d) return false;
            Object.defineProperty(layer, 'throws', {
                get() { throw new Error('own getter'); },
                set(value) { throw new Error('own setter'); }});
            let getterThrew = false, setterThrew = false;
            try { layer.throws; } catch (error) { getterThrew = error.message === 'own getter'; }
            try { layer.throws = 1; } catch (error) { setterThrew = error.message === 'own setter'; }
            return getterThrew && setterThrew;
        })())JS";
    }
    SECTION ("Known native fields remain active until explicitly shadowed by an own descriptor") {
	source = R"JS((function () {
            if (layer.known !== 42 || layer.missing !== undefined) return false;
            layer.known = 45;
            layer.undeclared = 19;
            if (layer.known !== 45 || layer.undeclared !== undefined) return false;
            Object.defineProperty(layer, 'known', {
                value: 77, writable: true, enumerable: true, configurable: true});
            if (layer.known !== 77) return false;
            layer.known = 78;
            return layer.known === 78 && Object.getOwnPropertyDescriptor(layer, 'known').value === 78;
        })())JS";
    }
    SECTION ("Native access does not invoke prototype Proxy traps") {
	source = R"JS((function () {
            let hasCalls = 0, getCalls = 0;
            Object.setPrototypeOf(layer, new Proxy({}, {
                has() { ++hasCalls; return true; },
                get() { ++getCalls; return 999; }}));
            if (layer.known !== 42 || layer.missing !== undefined) return false;
            layer.known = 45;
            Object.defineProperty(layer, 'custom', {value: 12});
            return layer.known === 45 && layer.custom === 12 && hasCalls === 0 && getCalls === 0;
        })())JS";
    }
    SECTION ("Ordinary prototype properties do not shadow native fields") {
	source = R"JS((function () {
            let calls = 0;
            Object.defineProperty(Object.prototype, 'known', {
                get() { ++calls; return 999; }, configurable: true});
            const initial = layer.known;
            Object.defineProperty(layer, 'known', {value: 77, configurable: true});
            const own = layer.known;
            delete layer.known;
            const restored = layer.known;
            delete Object.prototype.known;
            return initial === 42 && own === 77 && restored === 42 && calls === 0;
        })())JS";
    }
    REQUIRE (source != nullptr);
    JSValue result = JS_Eval (vm.context, source, strlen (source), "own-descriptors.js", JS_EVAL_TYPE_GLOBAL);
    const bool failed = JS_IsException (result);
    const bool passed = !failed && JS_ToBool (vm.context, result) == 1;
    JS_FreeValue (vm.context, result);
    CHECK_FALSE (failed);
    CHECK (passed);
    if (state.writes) {
	CHECK (state.known == 45);
	CHECK (state.writes == 1);
    }
}
