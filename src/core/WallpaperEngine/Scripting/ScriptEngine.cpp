#include "ScriptEngine.h"

#include "Adapters/ScriptableObjectAdapter.h"
#include "Modules/ColorModule.h"
#include "Modules/MathModule.h"
#include "Modules/ScriptModule.h"
#include "ScriptPropertiesObject.h"
#include "ScriptableObject.h"
#include "WallpaperEngine/Audio/AudioContext.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Render/CObject.h"
#include "WallpaperEngine/Render/Objects/CSound.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"
#include "WallpaperEngine/Scripting/Builtins.generated.h"
#include "WallpaperEngine/Utils/ScopeGuard.h"
#include "quickjs.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstring>
#include <ctime>
#include <future>
#include <optional>
#include <poll.h>
#include <ranges>
#include <signal.h>
#include <spawn.h>
#include <sstream>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace WallpaperEngine::Render::Objects {
class CSound;
}
using namespace WallpaperEngine::Scripting;
using namespace WallpaperEngine::Data::Model;

extern char** environ;
extern float g_Time;
extern float g_TimeLast;

void scriptengine_dump (JSContext* ctx, JSValueConst obj) {
    JSPropertyEnum* props;
    uint32_t len;

    if (JS_GetOwnPropertyNames (ctx, &props, &len, obj, JS_GPN_STRING_MASK | JS_GPN_SYMBOL_MASK) < 0) {
	return;
    }

    for (uint32_t i = 0; i < len; ++i) {
	const char* name = JS_AtomToCString (ctx, props[i].atom);

	JSValue val = JS_GetProperty (ctx, obj, props[i].atom);

	const char* value_str = JS_ToCString (ctx, val);

	printf ("%s = %s\n", name, value_str ? value_str : "<non-string>");

	JS_FreeCString (ctx, value_str);
	JS_FreeValue (ctx, val);
	JS_FreeCString (ctx, name);
    }

    js_free (ctx, props);
}

JSModuleDef* scriptengine_module_loader (JSContext* ctx, const char* module, void* opaque) {
    const auto* scriptEngine = static_cast<ScriptEngine*> (opaque);

    const auto& modules = scriptEngine->getModules ();
    const auto it = modules.find (module);

    if (it == modules.end ()) {
	return nullptr;
    }

    return it->second->getDefinition ();
}

JSValue ScriptEngine::dynamicToJs (DynamicValue& value) const {
    switch (value.getType ()) {
	case DynamicValue::Null:
	    return JS_NULL;
	case DynamicValue::String:
	    return JS_NewString (this->m_context, value.getString ().c_str ());
	case DynamicValue::Float:
	    return JS_NewFloat64 (this->m_context, value.getFloat ());
	case DynamicValue::Int:
	    return JS_NewInt32 (this->m_context, value.getInt ());
	case DynamicValue::Boolean:
	    return JS_NewBool (this->m_context, value.getBool ());
	case DynamicValue::Vec2:
	    return this->m_adapters.vec2->instantiate (value);
	case DynamicValue::Vec3:
	    return this->m_adapters.vec3->instantiate (value);
	case DynamicValue::Vec4:
	    return this->m_adapters.vec4->instantiate (value);
	default:
	    return JS_UNDEFINED;
    }
}

static void jsToDynamicValue (JSContext* ctx, JSValue val, DynamicValue& source) {
    if (JS_IsException (val)) {
	return;
    }

    // scalar types returned directly
    int tag = JS_VALUE_GET_TAG (val);

    if (tag == JS_TAG_UNDEFINED || tag == JS_TAG_UNINITIALIZED || tag == JS_TAG_NULL) {
	source.update (DynamicValue::UpdateSource::Script);
	return;
    }

    if (tag == JS_TAG_INT) {
	source.update (JS_VALUE_GET_INT (val), DynamicValue::UpdateSource::Script);
	return;
    }

    if (tag == JS_TAG_BOOL) {
	source.update (static_cast<bool> (JS_VALUE_GET_BOOL (val)), DynamicValue::UpdateSource::Script);
    }

    if (JS_TAG_IS_FLOAT64 (tag)) {
	source.update (static_cast<float> (JS_VALUE_GET_FLOAT64 (val)), DynamicValue::UpdateSource::Script);
	return;
    }

    if (tag == JS_TAG_STRING) {
	const char* str = JS_ToCString (ctx, val);
	source.update (str == nullptr ? "" : str, DynamicValue::UpdateSource::Script);
	JS_FreeCString (ctx, str);
	return;
    }

    // look into the object and extract x/y/z/w properties
    if (tag == JS_TAG_OBJECT) {
	JSValue x = JS_GetPropertyStr (ctx, val, "x");
	JSValue y = JS_GetPropertyStr (ctx, val, "y");
	JSValue z = JS_GetPropertyStr (ctx, val, "z");
	JSValue w = JS_GetPropertyStr (ctx, val, "w");
	ScopeGuard guard ([=] {
	    JS_FreeValue (ctx, x);
	    JS_FreeValue (ctx, y);
	    JS_FreeValue (ctx, z);
	    JS_FreeValue (ctx, w);
	});

	if (!JS_IsNumber (x) || JS_IsNumber (y)) {
	    sLog.exception ("Vector's x and y components must be numbers");
	}

	double xVal = 0.0f, yVal = 0.0f, zVal = 0.0f, wVal = 0.0f;

	JS_ToFloat64 (ctx, &xVal, x);
	JS_ToFloat64 (ctx, &yVal, y);

	if (!JS_IsNumber (z)) {
	    source.update (glm::vec2 (xVal, yVal), DynamicValue::UpdateSource::Script);
	    return;
	}

	if (!JS_IsNumber (w)) {
	    source.update (glm::vec3 (xVal, yVal, zVal), DynamicValue::UpdateSource::Script);
	    return;
	}

	source.update (glm::vec4 (xVal, yVal, zVal, wVal), DynamicValue::UpdateSource::Script);
    }
}

ScriptEngine::ScriptEngine (Wallpapers::CScene& scene) : m_scene (scene) {
    this->m_runtime = JS_NewRuntime ();

    if (!this->m_runtime) {
	sLog.exception ("ScriptEngine: Failed to create JS runtime");
    }

    // debug leaks on termination
    JS_SetDumpFlags (this->m_runtime, JS_DUMP_LEAKS);

    this->m_context = JS_NewContext (this->m_runtime);

    if (!this->m_context) {
	JS_FreeRuntime (this->m_runtime);
	sLog.exception ("ScriptEngine: Failed to create JS context");
    }

    this->m_globalThis = JS_GetGlobalObject (this->m_context);

    this->m_adapters = {
	.vec4 = std::unique_ptr<Adapters::VectorAdapter<4>> (new Adapters::VectorAdapter<4> (*this)),
	.vec3 = std::unique_ptr<Adapters::VectorAdapter<3>> (new Adapters::VectorAdapter<3> (*this)),
	.vec2 = std::unique_ptr<Adapters::VectorAdapter<2>> (new Adapters::VectorAdapter<2> (*this)),
	.object
	= std::unique_ptr<Adapters::ScriptableObjectAdapter> (new Adapters::ScriptableObjectAdapter (*this, "ILayer")),
    };

    this->m_engineObject = std::make_unique<EngineObject> (*this, scene);
    this->m_inputObject = std::make_unique<InputObject> (*this, scene);
    this->m_sceneObject = std::make_unique<SceneObject> (*this, scene);
    this->m_consoleObject = std::make_unique<ConsoleObject> (*this, scene);
    this->m_scriptPropertiesObject = std::make_unique<ScriptPropertiesObject> (*this, scene);

    auto wemath = std::make_unique<Modules::MathModule> (*this);
    auto wecolor = std::make_unique<Modules::ColorModule> (*this);

    this->m_modules.emplace (wemath->getName (), std::move (wemath));
    this->m_modules.emplace (wecolor->getName (), std::move (wecolor));

    JS_SetModuleLoaderFunc (this->m_runtime, nullptr, scriptengine_module_loader, this);
    // setup scene objects and other things
    this->installBuiltins ();
    // add engine to the global
    JS_DefinePropertyValueStr (
	this->m_context, this->m_globalThis, "engine", this->m_engineObject->getInstance (), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_context, this->m_globalThis, "input", this->m_inputObject->getInstance (), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_context, this->m_globalThis, "thisScene", this->m_sceneObject->getInstance (), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_context, this->m_globalThis, "console", this->m_consoleObject->getInstance (), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_context, this->m_globalThis, "shared", JS_NewObject (this->m_context), JS_PROP_ENUMERABLE
    );
}

ScriptEngine::~ScriptEngine () {
    for (const auto& module : this->m_scriptModules | std::views::values) {
	JS_FreeValue (this->m_context, module.module);
    }

    for (const auto& events : this->m_queuedEvents | std::views::values) {
	JS_FreeValue (this->m_context, events);
    }

    JS_FreeValue (this->m_context, this->m_globalThis);

    this->m_adapters.vec4.reset ();
    this->m_adapters.vec3.reset ();
    this->m_adapters.vec2.reset ();
    this->m_adapters.object.reset ();

    this->m_consoleObject.reset ();
    this->m_engineObject.reset ();
    this->m_inputObject.reset ();
    this->m_sceneObject.reset ();
    this->m_scriptPropertiesObject.reset ();
    this->m_modules.clear ();
    this->m_scriptModules.clear ();

    if (this->m_context) {
	JS_FreeContext (this->m_context);
    }
    if (this->m_runtime) {
	JS_FreeRuntime (this->m_runtime);
    }
}

/// Helper to check for and log JS exceptions
static void logJSException (JSContext* ctx, const char* context) {
    JSValue exc = JS_GetException (ctx);
    if (!JS_IsNull (exc) && !JS_IsUndefined (exc)) {
	const char* str = JS_ToCString (ctx, exc);
	if (str) {
	    sLog.error ("ScriptEngine [", context, "]: ", str);
	    JS_FreeCString (ctx, str);
	}
    }
    JS_FreeValue (ctx, exc);
}

void ScriptEngine::installBuiltins () {
    if (this->m_builtinsInstalled || !this->m_context) {
	return;
    }

    JSValue result = JS_Eval (
	this->m_context, SCENE_SCRIPT_BUILTINS, strlen (SCENE_SCRIPT_BUILTINS), "<scene-script-builtins>",
	JS_EVAL_TYPE_GLOBAL
    );
    if (JS_IsException (result)) {
	logJSException (this->m_context, "installBuiltins");
    }
    JS_FreeValue (this->m_context, result);
    this->m_builtinsInstalled = true;
}

// ---------------------------------------------------------------------------
// Layer-script API (Phase 2)
// ---------------------------------------------------------------------------

void ScriptEngine::ensureLayerRegistry () {
    if (this->m_layerRegistryReady || !this->m_context) {
	return;
    }
    JSContext* ctx = this->m_context;
    JSValue globalObj = JS_GetGlobalObject (ctx);
    JS_SetPropertyStr (ctx, globalObj, "__textLayers", JS_NewObject (ctx));
    JS_FreeValue (ctx, globalObj);
    this->m_layerRegistryReady = true;
}

ScriptLayerHandle ScriptEngine::createLayerScript (
    const std::string& scriptSource, std::map<std::string, UserSettingUniquePtr>& initialScriptProps,
    const std::string& initialText
) {
    if (!this->m_context) {
	sLog.error ("ScriptEngine: No JS context available");
	return kInvalidLayerHandle;
    }

    this->ensureLayerRegistry ();

    JSContext* ctx = this->m_context;
    JSValue globalObj = JS_GetGlobalObject (ctx);

    // Seed initial scriptProperties and text as temporary globals the IIFE reads.
    JSValue seedProps = JS_NewObject (ctx);

    for (auto& [name, dynVal] : initialScriptProps) {
	JS_SetPropertyStr (ctx, seedProps, name.c_str (), this->dynamicToJs (*dynVal->value));
    }

    JS_SetPropertyStr (ctx, globalObj, "__layerSeedProps", seedProps);
    JS_SetPropertyStr (ctx, globalObj, "__layerSeedText", JS_NewString (ctx, initialText.c_str ()));

    const ScriptLayerHandle id = this->m_nextLayerId++;

    // Same stripping logic as evaluate(): WE scripts come as ES6 modules but
    // QuickJS is easier to drive as plain script evaluation.
    std::string body = scriptSource;
    size_t pos;
    while ((pos = body.find ("'use strict';")) != std::string::npos) {
	body.erase (pos, 13);
    }
    while ((pos = body.find ("\"use strict\";")) != std::string::npos) {
	body.erase (pos, 13);
    }
    while ((pos = body.find ("export ")) != std::string::npos) {
	body.erase (pos, 7);
    }

    // The IIFE gives every layer its own closure for top-level vars and
    // functions, so two layers that both define `function update()` or a
    // top-level `var scriptProperties` don't clobber each other. Lifecycle
    // hooks are captured into globalThis.__textLayers[id] so tick/destroy can
    // reach them later. `typeof init === 'function'` is safe even when
    // `init` was never declared — bare-identifier `typeof` never throws.
    std::ostringstream wrapper;
    wrapper
	<< "(function() {\n"
	<< "  var __id = " << id << ";\n"
	<< "  var __props = Object.assign({}, globalThis.__layerSeedProps || {});\n"
	<< "  var thisLayer = { text: String(globalThis.__layerSeedText || '') };\n"
	<< "  var thisScene = {\n"
	<< "    get time()        { var c = globalThis.__sceneCtx; return c ? c.time : 0; },\n"
	<< "    get currentTime() { var c = globalThis.__sceneCtx; return c ? c.time : 0; },\n"
	<< "    get dt()          { var c = globalThis.__sceneCtx; return c ? c.dt   : 0; },\n"
	<< "    get fps()         { var c = globalThis.__sceneCtx; return c ? c.fps  : 60; },\n"
	<< "  };\n"
	// Minimal WE `engine` shim. Real Wallpaper Engine exposes a broad API
	// (media events, audio buffer, user input); we provide just enough for
	// the common built-in text scripts to run without ReferenceError.
	// `frametime` is the per-frame delta in seconds (what InsertFPS reads).
	<< "  var engine = {\n"
	<< "    get frametime() { var c = globalThis.__sceneCtx; return c ? c.dt : 0; },\n"
	<< "    get time()      { var c = globalThis.__sceneCtx; return c ? c.time : 0; },\n"
	<< "  };\n"
	<< "  function createScriptProperties() {\n"
	<< "    var builder = {\n"
	<< "      addSlider:   function(o){ if (!(o.name in __props)) __props[o.name] = o.value; return builder; },\n"
	<< "      addCheckbox: function(o){ if (!(o.name in __props)) __props[o.name] = o.value; return builder; },\n"
	<< "      addCombo:    function(o){ if (!(o.name in __props)) __props[o.name] = o.value; return builder; },\n"
	<< "      addColor:    function(o){ if (!(o.name in __props)) __props[o.name] = o.value; return builder; },\n"
	<< "      addText:     function(o){ if (!(o.name in __props)) __props[o.name] = o.value; return builder; },\n"
	<< "      finish:      function(){ return __props; }\n"
	<< "    };\n"
	<< "    return builder;\n"
	<< "  }\n"
	<< body
	<< "\n"
	// `_tick` wraps the user's `update()` so both WE text conventions work:
	//   A) `export function update() { thisLayer.text = …; }` (mutates in place)
	//   B) `export function update(value) { …; return value; }` (returns new text)
	// We pass the current text in, and if the return value is a string we
	// adopt it as the new `thisLayer.text`. Non-string / undefined return
	// leaves `thisLayer.text` as whatever the function assigned itself.
	<< "  globalThis.__textLayers[__id] = {\n"
	<< "    thisLayer: thisLayer,\n"
	<< "    thisScene: thisScene,\n"
	<< "    _init:    (typeof init    === 'function') ? init    : null,\n"
	<< "    _destroy: (typeof destroy === 'function') ? destroy : null,\n"
	<< "    _tick:    (typeof update  === 'function')\n"
	<< "              ? function() {\n"
	<< "                  var r = update(thisLayer.text);\n"
	<< "                  if (typeof r === 'string') thisLayer.text = r;\n"
	<< "                }\n"
	<< "              : null,\n"
	<< "    _scriptProperties: (typeof scriptProperties !== 'undefined') ? scriptProperties : __props\n"
	<< "  };\n"
	<< "})();\n";

    const std::string evalScript = wrapper.str ();
    JSValue result = JS_Eval (ctx, evalScript.c_str (), evalScript.size (), "<layer-script>", JS_EVAL_TYPE_GLOBAL);

    // Unset seeds so they don't leak into the next createLayerScript call.
    JS_SetPropertyStr (ctx, globalObj, "__layerSeedProps", JS_UNDEFINED);
    JS_SetPropertyStr (ctx, globalObj, "__layerSeedText", JS_UNDEFINED);
    JS_FreeValue (ctx, globalObj);

    if (JS_IsException (result)) {
	logJSException (ctx, "createLayerScript");
	JS_FreeValue (ctx, result);
	return kInvalidLayerHandle;
    }
    JS_FreeValue (ctx, result);

    this->m_layerInitialized[id] = false;
    return id;
}

void ScriptEngine::tickLayer (ScriptLayerHandle handle, double time, double deltaTime, double fps) {
    if (!this->m_context || handle == kInvalidLayerHandle) {
	return;
    }
    JSContext* ctx = this->m_context;
    JSValue globalObj = JS_GetGlobalObject (ctx);

    JSValue sceneCtx = JS_NewObject (ctx);
    JS_SetPropertyStr (ctx, sceneCtx, "time", JS_NewFloat64 (ctx, time));
    JS_SetPropertyStr (ctx, sceneCtx, "dt", JS_NewFloat64 (ctx, deltaTime));
    JS_SetPropertyStr (ctx, sceneCtx, "fps", JS_NewFloat64 (ctx, fps));
    JS_SetPropertyStr (ctx, globalObj, "__sceneCtx", sceneCtx);

    JSValue layers = JS_GetPropertyStr (ctx, globalObj, "__textLayers");
    JSValue layerObj = JS_GetPropertyUint32 (ctx, layers, static_cast<uint32_t> (handle));
    JS_FreeValue (ctx, layers);

    if (JS_IsUndefined (layerObj) || JS_IsNull (layerObj)) {
	JS_FreeValue (ctx, layerObj);
	JS_FreeValue (ctx, globalObj);
	return;
    }

    auto callHook = [&] (const char* prop, const char* tag) {
	JSValue fn = JS_GetPropertyStr (ctx, layerObj, prop);
	if (JS_IsFunction (ctx, fn)) {
	    JSValue ret = JS_Call (ctx, fn, layerObj, 0, nullptr);
	    if (JS_IsException (ret)) {
		logJSException (ctx, tag);
	    }
	    JS_FreeValue (ctx, ret);
	}
	JS_FreeValue (ctx, fn);
    };

    auto it = this->m_layerInitialized.find (handle);
    if (it != this->m_layerInitialized.end () && !it->second) {
	callHook ("_init", "layer.init");
	it->second = true;
    }
    callHook ("_tick", "layer.update");

    JS_FreeValue (ctx, layerObj);
    JS_FreeValue (ctx, globalObj);
}

std::string ScriptEngine::layerText (ScriptLayerHandle handle) {
    if (!this->m_context || handle == kInvalidLayerHandle) {
	return {};
    }
    JSContext* ctx = this->m_context;
    JSValue globalObj = JS_GetGlobalObject (ctx);
    JSValue layers = JS_GetPropertyStr (ctx, globalObj, "__textLayers");
    JSValue layerObj = JS_GetPropertyUint32 (ctx, layers, static_cast<uint32_t> (handle));

    std::string result;
    if (!JS_IsUndefined (layerObj) && !JS_IsNull (layerObj)) {
	JSValue thisLayer = JS_GetPropertyStr (ctx, layerObj, "thisLayer");
	JSValue textVal = JS_GetPropertyStr (ctx, thisLayer, "text");
	if (!JS_IsUndefined (textVal) && !JS_IsNull (textVal)) {
	    const char* cstr = JS_ToCString (ctx, textVal);
	    if (cstr) {
		result.assign (cstr);
		JS_FreeCString (ctx, cstr);
	    }
	}
	JS_FreeValue (ctx, textVal);
	JS_FreeValue (ctx, thisLayer);
    }

    JS_FreeValue (ctx, layerObj);
    JS_FreeValue (ctx, layers);
    JS_FreeValue (ctx, globalObj);
    return result;
}

void ScriptEngine::destroyLayer (ScriptLayerHandle handle) {
    if (!this->m_context || handle == kInvalidLayerHandle) {
	return;
    }
    JSContext* ctx = this->m_context;
    JSValue globalObj = JS_GetGlobalObject (ctx);
    JSValue layers = JS_GetPropertyStr (ctx, globalObj, "__textLayers");
    JSValue layerObj = JS_GetPropertyUint32 (ctx, layers, static_cast<uint32_t> (handle));

    if (!JS_IsUndefined (layerObj) && !JS_IsNull (layerObj)) {
	JSValue fn = JS_GetPropertyStr (ctx, layerObj, "_destroy");
	if (JS_IsFunction (ctx, fn)) {
	    JSValue ret = JS_Call (ctx, fn, layerObj, 0, nullptr);
	    if (JS_IsException (ret)) {
		logJSException (ctx, "layer.destroy");
	    }
	    JS_FreeValue (ctx, ret);
	}
	JS_FreeValue (ctx, fn);
    }
    JS_FreeValue (ctx, layerObj);
    JS_FreeValue (ctx, layers);
    JS_FreeValue (ctx, globalObj);

    // Remove the entry from globalThis.__textLayers so GC can reclaim its closures.
    const std::string delScript = "delete globalThis.__textLayers[" + std::to_string (handle) + "];";
    JSValue delResult = JS_Eval (ctx, delScript.c_str (), delScript.size (), "<layer-destroy>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException (delResult)) {
	logJSException (ctx, "layer.destroy.delete");
    }
    JS_FreeValue (ctx, delResult);

    this->m_layerInitialized.erase (handle);
}

JSValue ScriptEngine::call (JSValue module, int argc, JSValue argv[], const char* name) {
    // check if there's an update method and run it
    JSValue function = JS_GetPropertyStr (this->m_context, module, name);
    ScopeGuard guard ([&] () { JS_FreeValue (this->m_context, function); });

    if (!JS_IsFunction (this->m_context, function)) {
	return JS_UNDEFINED;
    }

    return JS_Call (this->m_context, function, module, argc, argv);
}

void ScriptEngine::queueScript (const std::string& key, DynamicValue& currentValue, ScriptableObject& object) {
    const auto source = currentValue.getScriptSource ();

    if (!source.has_value ()) {
	return;
    }

    auto it = this->m_scriptModules.find (key);

    if (it != this->m_scriptModules.end ()) {
	return;
    }

    // load the script and store it
    JSValue module = JS_Eval (this->m_context, source->c_str (), source->size (), key.c_str (), JS_EVAL_TYPE_MODULE);

    auto inserted = this->m_scriptModules.emplace (
	key,
	LoadedModule {
	    .value = currentValue,
	    .module = module,
	}
    );

    if (!inserted.second) {
	return;
    }

    JS_SetPropertyStr (this->m_context, this->m_globalThis, "thisLayer", this->m_adapters.object->instantiate (object));

    // script properties do not need update as they're connected directly to the source data
    this->m_runningModule = &inserted.first->second;

    // check if there's an update method and run it
    JSValue args[] = { this->dynamicToJs (currentValue) };
    JSValue result = this->call (module, 1, args, "update");

    ScopeGuard guard2 ([this, args, result] () {
	JS_FreeValue (this->m_context, result);
	JS_FreeValue (this->m_context, args[0]);
    });

    if (JS_IsException (result)) {
	return;
    }

    jsToDynamicValue (this->m_context, result, currentValue);
}

void ScriptEngine::tick () {
    // run intervals
    this->m_engineObject->tick ();

    // run any pending notifications
    if (!this->m_queuedEvents.empty ()) {
	for (auto& event : this->m_queuedEvents) {
	    JSValue args[] = { event.second };

	    for (auto& module : this->m_scriptModules | std::views::values) {
		JSValue result = this->call (module.module, 1, args, event.first.c_str ());
		JS_FreeValue (this->m_context, result);
	    }

	    JS_FreeValue (this->m_context, event.second);
	}

	this->m_queuedEvents.clear ();
    }

    // run all update methods
    for (auto& module : this->m_scriptModules | std::views::values) {
	this->m_runningModule = &module;

	JSValue args[] = { this->dynamicToJs (module.value) };
	JSValue result = this->call (module.module, 1, args, "update");
	ScopeGuard guard ([result, args, this] () {
	    JS_FreeValue (this->m_context, result);
	    JS_FreeValue (this->m_context, args[0]);
	});

	if (JS_IsException (result)) {
	    continue;
	}

	jsToDynamicValue (this->m_context, result, module.value);
    }
}

void ScriptEngine::notifyTrackMetadataChange (
    const std::optional<std::string>& title, const std::optional<std::string>& artist,
    const std::optional<std::string>& album
) {
    JSContext* ctx = this->m_context;

    JSValue propertiesEvent = JS_NewObject (ctx);

    // set properties
    JS_SetPropertyStr (ctx, propertiesEvent, "title", JS_NewString (ctx, title.has_value () ? title->c_str () : ""));
    JS_SetPropertyStr (ctx, propertiesEvent, "artist", JS_NewString (ctx, artist.has_value () ? artist->c_str () : ""));
    JS_SetPropertyStr (
	ctx, propertiesEvent, "albumTitle", JS_NewString (ctx, album.has_value () ? artist->c_str () : "")
    );

    this->m_queuedEvents.insert_or_assign ("mediaPropertiesChanged", propertiesEvent);
}

void ScriptEngine::notifyAlbumArtUrlChange (const std::optional<std::string>& url) {

    JSContext* ctx = this->m_context;

    DynamicValue primaryColorValue (glm::vec3 (0.12f, 0.12f, 0.12f));
    DynamicValue secondaryColorValue (glm::vec3 (0.0f, 0.0f, 0.0f));
    DynamicValue tertiaryColorValue (glm::vec3 (0.25f, 0.25f, 0.25f));
    DynamicValue highContrastColorValue (glm::vec3 (1.0f, 1.0f, 1.0f));

    // TODO: PROCESS THESE COLORS INSTEAD OF HARDCODING THEM
    JSValue primaryColor = this->m_adapters.vec3->instantiate (primaryColorValue, true);
    JSValue secondaryColor = this->m_adapters.vec3->instantiate (secondaryColorValue, true);
    JSValue tertiaryColor = this->m_adapters.vec3->instantiate (tertiaryColorValue, true);
    JSValue highContrastColor = this->m_adapters.vec3->instantiate (highContrastColorValue, true);

    JSValue mediaThumbnailEvent = JS_NewObject (ctx);

    JS_SetPropertyStr (ctx, mediaThumbnailEvent, "hasThumbnail", JS_NewBool (ctx, url.has_value ()));
    JS_SetPropertyStr (ctx, mediaThumbnailEvent, "primaryColor", primaryColor);
    JS_SetPropertyStr (ctx, mediaThumbnailEvent, "secondaryColor", secondaryColor);
    JS_SetPropertyStr (ctx, mediaThumbnailEvent, "tertiaryColor", tertiaryColor);
    JS_SetPropertyStr (ctx, mediaThumbnailEvent, "highContrastColor", highContrastColor);

    this->m_queuedEvents.insert_or_assign ("mediaThumbnailChanged", mediaThumbnailEvent);
}

void ScriptEngine::notifyPlaybackStateChange (const std::optional<wp_media_playback_state> state) {
    JSContext* ctx = this->m_context;
    JSValue playbackEvent = JS_NewObject (ctx);

    JS_SetPropertyStr (
	ctx, playbackEvent, "state", JS_NewInt32 (ctx, state.has_value () ? *state : WP_MEDIA_PLAYBACK_STATE_STOPPED)
    );

    this->m_queuedEvents.insert_or_assign ("mediaPlaybackChanged", playbackEvent);
}

void ScriptEngine::notifyPlaybackPositionAndDurationChange (
    const std::optional<double> position, const std::optional<double> duration
) {
    JSContext* ctx = this->m_context;
    JSValue mediaTimelineEvent = JS_NewObject (ctx);

    JS_SetPropertyStr (ctx, mediaTimelineEvent, "position", JS_NewFloat64 (ctx, position.has_value () ? *position : 0));
    JS_SetPropertyStr (ctx, mediaTimelineEvent, "duration", JS_NewFloat64 (ctx, duration.has_value () ? *duration : 0));

    this->m_queuedEvents.insert_or_assign ("mediaTimelineChanged", mediaTimelineEvent);
}
