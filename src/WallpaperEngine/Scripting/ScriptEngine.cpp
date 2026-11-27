#include "ScriptEngine.h"
#include "LocalStorage.h"

#include "Adapters/ScriptableObjectAdapter.h"
#include "Adapters/VideoTextureAdapter.h"
#include "Modules/ColorModule.h"
#include "Modules/MathModule.h"
#include "Modules/VectorModule.h"
#include "Modules/ScriptModule.h"
#include "ScriptPropertiesObject.h"
#include "ScriptableObject.h"
#include "WallpaperEngine/Application/WallpaperApplication.h"
#include "WallpaperEngine/Audio/AudioContext.h"
#include "WallpaperEngine/Audio/Drivers/Recorders/PlaybackRecorder.h"
#include "WallpaperEngine/Data/Model/DynamicValue.h"
// Property is only forward-declared through Types.h; dispatchAllUserProperties needs the full
// definition to see that it derives from DynamicValue.
#include "WallpaperEngine/Data/Model/Property.h"
#include "WallpaperEngine/Data/Model/UserSetting.h"
#include "WallpaperEngine/Data/Utils/ScopeGuard.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Render/CObject.h"
#include "WallpaperEngine/Render/Objects/CImage.h"
#include "WallpaperEngine/Render/Objects/CModel.h"
#include "WallpaperEngine/Render/Objects/CSound.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"
#include "WallpaperEngine/Scripting/Builtins.generated.h"
#include "quickjs.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <ctime>
#include <future>
#include <optional>
#include <poll.h>
#include <ranges>
#include <regex>
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
	JS_FreeAtom (ctx, props[i].atom);
    }

    js_free (ctx, props);
}

JSModuleDef* scriptengine_module_loader (JSContext* ctx, const char* module, void* opaque) {
    const auto* scriptEngine = static_cast<ScriptEngine*> (opaque);

    const auto& modules = scriptEngine->getModules ();
    const auto it = modules.find (module);

    if (it == modules.end ()) {
	JS_ThrowReferenceError (ctx, "could not find module '%s'", module);
	return nullptr;
    }

    return it->second->getDefinition ();
}

JSValue ScriptEngine::dynamicToJs (DynamicValue& value, const bool snapshotVectors) const {
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
	    return snapshotVectors ? this->m_adapters.vec2->instantiate (value, true)
		: this->m_adapters.vec2->instantiate (value);
	case DynamicValue::Vec3:
	    return snapshotVectors ? this->m_adapters.vec3->instantiate (value, true)
		: this->m_adapters.vec3->instantiate (value);
	case DynamicValue::Vec4:
	    return snapshotVectors ? this->m_adapters.vec4->instantiate (value, true)
		: this->m_adapters.vec4->instantiate (value);
	default:
	    return JS_UNDEFINED;
    }
}

// Wallpaper Engine's SceneScript API exposes object angles in degrees while the scene
// data stores radians (see the authored values vs. the angle scripts in e.g. workshop
// scene 3589454154). Angle properties therefore cross the script boundary as
// independent degree-valued snapshots.
static bool isAnglesProperty (const std::string& key) { return key.rfind ("angles_", 0) == 0; }

struct LoweredSceneScript {
    std::string body;
    bool exportsWorkshopId;
};

static LoweredSceneScript normalizeSceneScriptModuleSyntax (const std::string& source) {
    // Workshop scripts may put an aliased import in the middle of a minified
    // line: `setup();import*as math from'WEMath';export function update...`.
    // Lower declarations, not entire lines or text inside strings/comments.
    static const std::regex namespaceImport (
	R"(import\s*\*\s*as\s+([a-zA-Z_$][a-zA-Z0-9_$]*)\s+from\s*(['"])(WEMath|WEVector|WEColor)\2\s*;?)"
    );
    const auto unicodeWhitespaceLength = [&] (const size_t pos) -> size_t {
	const std::string_view remaining (source.data () + pos, source.size () - pos);
	// JavaScript permits these UTF-8 spaces between keywords. Do not fold
	// them into identifiers, or `export\u00a0function` survives module lowering.
	if (remaining.starts_with ("\u00a0")) return 2;
	for (const std::string_view space : { "\u1680", "\u2000", "\u2001", "\u2002", "\u2003",
	    "\u2004", "\u2005", "\u2006", "\u2007", "\u2008", "\u2009", "\u200a",
	    "\u2028", "\u2029", "\u202f", "\u205f", "\u3000", "\ufeff" }) {
	    if (remaining.starts_with (space)) return 3;
	}
	return 0;
    };
    const auto identifier = [&] (const size_t pos) {
	const unsigned char c = source[pos];
	return std::isalnum (c) || c == '_' || c == '$'
	    || (c >= 128 && unicodeWhitespaceLength (pos) == 0);
    };
    std::string output, imports;
    enum class ExportPrefix { None, Keyword, Declaration };
    auto exportPrefix = ExportPrefix::None;
    bool exportsWorkshopId = false;
    int depth = 0;
    bool expressionExpected = true;
    for (size_t pos = 0; pos < source.size ();) {
	const size_t begin = pos;
	const char c = source[pos++];
	if (static_cast<unsigned char> (c) >= 128 && unicodeWhitespaceLength (begin) != 0) {
	    pos = begin + unicodeWhitespaceLength (begin);
	} else if (c == '\'' || c == '"' || c == '`') {
	    while (pos < source.size ()) {
		const char next = source[pos++];
		if (next == '\\' && pos < source.size ()) ++pos;
		else if (next == c) break;
	    }
	    exportPrefix = ExportPrefix::None;
	    expressionExpected = false;
	} else if (c == '/' && pos < source.size () && source[pos] == '/') {
	    while (pos < source.size () && source[pos] != '\n') ++pos;
	} else if (c == '/' && pos < source.size () && source[pos] == '*') {
	    const size_t end = source.find ("*/", pos + 1);
	    pos = end == std::string::npos ? source.size () : end + 2;
	} else if (c == '/' && expressionExpected) {
	    bool characterClass = false;
	    while (pos < source.size ()) {
		const char next = source[pos++];
		if (next == '\\' && pos < source.size ()) ++pos;
		else if (next == '[') characterClass = true;
		else if (next == ']') characterClass = false;
		else if (next == '/' && !characterClass) break;
	    }
	    while (pos < source.size () && identifier (pos)) ++pos;
	    exportPrefix = ExportPrefix::None;
	    expressionExpected = false;
	} else if (identifier (begin)) {
	    while (pos < source.size () && identifier (pos)) ++pos;
	    const std::string_view token (source.data () + begin, pos - begin);
	    if (depth == 0 && token == "export") {
		exportPrefix = ExportPrefix::Keyword;
		expressionExpected = true;
		continue;
	    }
	    if (exportPrefix == ExportPrefix::Keyword && (token == "let" || token == "const" || token == "var")) {
		exportPrefix = ExportPrefix::Declaration;
	    } else {
		exportsWorkshopId |= exportPrefix == ExportPrefix::Declaration && token == "__workshopId";
		exportPrefix = ExportPrefix::None;
	    }
	    if (depth == 0 && token == "import") {
		std::match_results<std::string::const_iterator> match;
		if (std::regex_search (source.begin () + begin, source.end (), match, namespaceImport,
				      std::regex_constants::match_continuous)) {
		    imports += "const " + match[1].str () + " = globalThis." + match[3].str () + ";\n";
		    pos = begin + match.length ();
		    // Keep the original line numbers for script diagnostics.
		    output.append (std::count (source.begin () + begin, source.begin () + pos, '\n'), '\n');
		    expressionExpected = true;
		    continue;
		}
	    }
	    expressionExpected = token == "return" || token == "throw" || token == "case"
		|| token == "typeof" || token == "void" || token == "delete" || token == "yield"
		|| token == "await" || token == "in" || token == "instanceof";
	} else if (!std::isspace (static_cast<unsigned char> (c))) {
	    exportPrefix = ExportPrefix::None;
	    if (c == '{') ++depth;
	    if (c == '}') --depth;
	    expressionExpected = c != ')' && c != ']' && c != '.';
	}
	output.append (source, begin, pos - begin);
    }
    return { imports + output, exportsWorkshopId };
}

static JSValue anglesToJs (WallpaperEngine::Scripting::Adapters::VectorAdapter<3>& adapter, const DynamicValue& value) {
    DynamicValue degrees (glm::degrees (value.getVec3 ()));
    return adapter.instantiate (degrees, true);
}

static void jsToAngles (JSContext* ctx, JSValue result, JSValue argument, DynamicValue& source,
			const glm::vec3& initialDegrees) {
    // scripts either mutate the passed value, return a new one, or both
    const JSValue val = JS_IsObject (result) ? result : argument;

    if (!JS_IsObject (val)) {
	return;
    }

    JSValue x = JS_GetPropertyStr (ctx, val, "x");
    JSValue y = JS_GetPropertyStr (ctx, val, "y");
    JSValue z = JS_GetPropertyStr (ctx, val, "z");
    ScopeGuard guard ([=] {
	JS_FreeValue (ctx, x);
	JS_FreeValue (ctx, y);
	JS_FreeValue (ctx, z);
    });

    if (!JS_IsNumber (x) || !JS_IsNumber (y) || !JS_IsNumber (z)) {
	return;
    }

    double xVal = 0.0, yVal = 0.0, zVal = 0.0;
    JS_ToFloat64 (ctx, &xVal, x);
    JS_ToFloat64 (ctx, &yVal, y);
    JS_ToFloat64 (ctx, &zVal, z);

    if (std::isnan (xVal) || std::isnan (yVal) || std::isnan (zVal)) {
	return;
    }

    const glm::vec3 degrees (xVal, yVal, zVal);
    // This angle-specific fallback accepts in-place edits to the degree snapshot.
    // An unchanged snapshot must not overwrite direct layer assignments or rotateObjectSpace() calls.
    if (!JS_IsObject (result) && degrees == initialDegrees) return;
    source.update (glm::radians (degrees), DynamicValue::UpdateSource::Script);
}

static void jsToDynamicValue (JSContext* ctx, JSValue val, DynamicValue& source) {
    if (JS_IsException (val)) {
	return;
    }

    // scalar types returned directly
    int tag = JS_VALUE_GET_TAG (val);

    if (tag == JS_TAG_UNDEFINED || tag == JS_TAG_UNINITIALIZED || tag == JS_TAG_NULL) {
	// No return value means "keep the current value". Callback vector arguments
	// are snapshots; mutating them alone does not write back to the property.
	// Missing hooks also return undefined through call().
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
	// A const char* would select update(bool), silently discarding the text.
	source.update (std::string (str == nullptr ? "" : str), DynamicValue::UpdateSource::Script);
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

	if (!JS_IsNumber (x) || !JS_IsNumber (y)) {
	    // scripts can transiently return garbage (e.g. NaN math while another script's
	    // shared values aren't populated yet); keep the previous value instead of dying
	    return;
	}

	double xVal = 0.0f, yVal = 0.0f, zVal = 0.0f, wVal = 0.0f;

	JS_ToFloat64 (ctx, &xVal, x);
	JS_ToFloat64 (ctx, &yVal, y);

	if (!JS_IsNumber (z)) {
	    source.update (glm::vec2 (xVal, yVal), DynamicValue::UpdateSource::Script);
	    return;
	}

	JS_ToFloat64 (ctx, &zVal, z);

	if (!JS_IsNumber (w)) {
	    source.update (glm::vec3 (xVal, yVal, zVal), DynamicValue::UpdateSource::Script);
	    return;
	}

	JS_ToFloat64 (ctx, &wVal, w);
	source.update (glm::vec4 (xVal, yVal, zVal, wVal), DynamicValue::UpdateSource::Script);
    }
}

ScriptEngine::ScriptEngine (Wallpapers::CScene& scene, Media::MediaSource& mediaSource, const std::string& screenName) :
    m_scene (scene), m_mediaSource (mediaSource) {
    this->m_unregisterMediaUpdateCallback
	= mediaSource.addMetadataListener ([this] (const Media::MediaSource::MediaInfo& info) {
	      this->notifyMediaUpdate (info);
	  });

    this->m_unregisterAlbumArtUpdateCallback
	= mediaSource.addAlbumArtListener ([this] (const Media::MediaSource::MediaInfo& info) {
	      this->notifyMediaUpdate (info);
	  });

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
	.video = std::make_unique<Adapters::VideoTextureAdapter> (*this),
    };

    this->m_engineObject = std::make_unique<EngineObject> (*this, scene);
    this->m_inputObject = std::make_unique<InputObject> (*this, scene);
    this->m_sceneObject = std::make_unique<SceneObject> (*this, scene);
    this->m_consoleObject = std::make_unique<ConsoleObject> (*this, scene);
    this->m_scriptPropertiesObject = std::make_unique<ScriptPropertiesObject> (*this, scene);

    auto wemath = std::make_unique<Modules::MathModule> (*this);
    auto wecolor = std::make_unique<Modules::ColorModule> (*this);
    auto wevector = std::make_unique<Modules::VectorModule> (*this);

    this->m_modules.emplace (wemath->getName (), std::move (wemath));
    this->m_modules.emplace (wecolor->getName (), std::move (wecolor));
    this->m_modules.emplace (wevector->getName (), std::move (wevector));

    JS_SetModuleLoaderFunc (this->m_runtime, nullptr, scriptengine_module_loader, this);
    // Use the physical project path, not the mount fingerprint (which includes
    // unrelated stock assets and can change when the application is updated).
    std::string storageProject = scene.getAssetLocator ().identity ();
    try {
        storageProject = std::filesystem::weakly_canonical (
            scene.getAssetLocator ().physicalPath ("project.json")
        ).string ();
    } catch (const std::filesystem::filesystem_error&) {
        // Virtual/package-only projects have no physical project.json path.
    }
    installLocalStorage (this->m_context, this->m_globalThis, storageProject, screenName);
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
	this->m_context, this->m_globalThis, "console", this->m_consoleObject->getInstance (),
	JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_context, this->m_globalThis, "shared", JS_NewObject (this->m_context),
	JS_PROP_ENUMERABLE | JS_PROP_WRITABLE
    );
}

ScriptEngine::~ScriptEngine () {
    this->m_unregisterMediaUpdateCallback ();
    // also drop the album-art listener: it captures `this`, and scenes are destroyed and recreated
    // on every in-process wallpaper rebuild — a leaked registration turns the next album-art update
    // into a call through a dangling pointer
    this->m_unregisterAlbumArtUpdateCallback ();

    for (const auto& module : this->m_scriptModules | std::views::values) {
	JS_FreeValue (this->m_context, module.module);
    }

    // Do not mutate globalThis while tearing a context down. Some authored globals
    // are non-writable; JS_SetPropertyStr then enters QuickJS's exception path while
    // CScene is already unwinding, which is the switch crash seen in core dumps.
    // JS_FreeContext/JS_FreeRuntime release the context-owned global graph directly.
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
    this->m_scriptModuleOrder.clear ();
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
    if (JS_IsUninitialized (exc)) {
	JS_FreeValue (ctx, exc);
	return;
    }
    if (!JS_IsNull (exc) && !JS_IsUndefined (exc)) {
	const char* str = JS_ToCString (ctx, exc);
	if (str) {
	    sLog.error ("ScriptEngine [", context, "]: ", str);
	    JS_FreeCString (ctx, str);
	}
	JSValue stack = JS_GetPropertyStr (ctx, exc, "stack");
	if (!JS_IsUndefined (stack) && !JS_IsNull (stack)) {
	    const char* sstr = JS_ToCString (ctx, stack);
	    if (sstr) {
		sLog.error ("ScriptEngine [", context, "] stack: ", sstr);
		JS_FreeCString (ctx, sstr);
	    }
	}
	JS_FreeValue (ctx, stack);
    }
    JS_FreeValue (ctx, exc);
}

void ScriptEngine::installBuiltins () {
    if (this->m_builtinsInstalled || !this->m_context) {
	return;
    }

    JSValue result = JS_Eval (
        m_context, SCENE_SCRIPT_VECTORS, strlen (SCENE_SCRIPT_VECTORS), "<scene-script-vectors>", JS_EVAL_TYPE_GLOBAL
    );
    if (JS_IsException (result)) {
        logJSException (m_context, "installVectors");
    } else {
        m_adapters.vec2->useScriptConstructor ();
        m_adapters.vec3->useScriptConstructor ();
        m_adapters.vec4->useScriptConstructor ();
    }
    JS_FreeValue (m_context, result);

    result = JS_Eval (
	this->m_context, SCENE_SCRIPT_BUILTINS, strlen (SCENE_SCRIPT_BUILTINS), "<scene-script-builtins>",
	JS_EVAL_TYPE_GLOBAL
    );
    if (JS_IsException (result)) {
	logJSException (this->m_context, "installBuiltins");
    }
    JS_FreeValue (this->m_context, result);

    // Property scripts run through the lowered wrapper, so their removed
    // imports never instantiate C modules. Publish the real color module once
    // per context and let the wrapper bind its namespace like WEMath/WEVector.
#ifdef WPENGINE_JAVASCRIPTCORE
    result = JS_GetModuleNamespace (m_context, m_modules.at ("WEColor")->getDefinition ());
    if (!JS_IsException (result)) {
        JS_SetPropertyStr (m_context, m_globalThis, "WEColor", JS_DupValue (m_context, result));
    }
#else
    constexpr char colorModule[] = "import * as WEColor from 'WEColor'; globalThis.WEColor = WEColor;";
    result = JS_Eval (
	this->m_context, colorModule, sizeof (colorModule) - 1, "<scene-script-color-module>", JS_EVAL_TYPE_MODULE
    );
#endif
    if (JS_IsException (result)) {
	logJSException (this->m_context, "installColorModule");
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

    // WE scripts arrive as ES modules; run them as isolated plain scripts.
    const std::string body = normalizeSceneScriptModuleSyntax (scriptSource).body;

    // The IIFE gives every layer its own closure for top-level vars and
    // functions, so two layers that both define `function update()` or a
    // top-level `var scriptProperties` don't clobber each other. Lifecycle
    // hooks are captured into globalThis.__textLayers[id] so tick/destroy can
    // reach them later. `typeof init === 'function'` is safe even when
    // `init` was never declared — bare-identifier `typeof` never throws.
    std::ostringstream wrapper;
    wrapper
	<< "(function() {\n"
	// As with property scripts, this is an ES-module body lowered into a classic IIFE.
	// Keep module strictness across generated code that precedes the authored directive.
	<< "  'use strict';\n"
	<< "  var __id = " << id << ";\n"
	<< "  var __props = Object.assign({}, globalThis.__layerSeedProps || {});\n"
	<< "  var thisLayer = { text: String(globalThis.__layerSeedText || '') };\n"
	<< "  var thisScene = {\n"
	<< "    get time()        { var c = globalThis.__sceneCtx; return c ? c.time : 0; },\n"
	<< "    get currentTime() { var c = globalThis.__sceneCtx; return c ? c.time : 0; },\n"
	<< "    get dt()          { var c = globalThis.__sceneCtx; return c ? c.dt   : 0; },\n"
	<< "    get fps()         { var c = globalThis.__sceneCtx; return c ? c.fps  : 60; },\n"
	<< "  };\n"
	// Layer scripts get the real `engine` object, with only the per-frame clock layered on
	// top. A self-contained shim here used to shadow the global one, so anything a layer
	// script reached for beyond these two getters - registerAsset above all, which scripts
	// call at module scope to declare their fonts - threw "not a function" and took the whole
	// layer down with it. `frametime` is the per-frame delta in seconds (what InsertFPS reads).
	<< "  var engine = Object.create(globalThis.engine || Object.prototype, {\n"
	<< "    frametime: { get: function() { var c = globalThis.__sceneCtx; return c ? c.dt : 0; } },\n"
	<< "    time:      { get: function() { var c = globalThis.__sceneCtx; return c ? c.time : 0; } },\n"
	<< "  });\n"
	<< "  function createScriptProperties() {\n"
	<< "    var builder = {\n"
	<< "      addSlider:   function(o){ if (!(o.name in __props)) __props[o.name] = o.value; return builder; },\n"
	<< "      addCheckbox: function(o){ if (!(o.name in __props)) __props[o.name] = o.value; return builder; },\n"
	<< "      addCombo:    function(o){ var value = o.options[0].value;\n"
	<< "        if (!(o.name in __props)) __props[o.name] = value; return builder; },\n"
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
    std::map<std::string, bool> registeredAssets;
    JSValue result;
    {
	auto* previousAssets = this->m_registeringAssets;
	ScopeGuard restoreAssets ([&] { this->m_registeringAssets = previousAssets; });
	this->m_registeringAssets = &registeredAssets;
	result = JS_Eval (ctx, evalScript.c_str (), evalScript.size (), "<layer-script>", JS_EVAL_TYPE_GLOBAL);
    }

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
	const bool previousUpdate = m_inUpdate;
	ScopeGuard restoreUpdate ([&] { m_inUpdate = previousUpdate; });
	m_inUpdate = previousUpdate || std::string_view (prop) == "_tick";
	auto* previousAssets = this->m_registeringAssets;
	ScopeGuard restoreAssets ([&] { this->m_registeringAssets = previousAssets; });
	this->m_registeringAssets = nullptr;
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
	    auto* previousAssets = this->m_registeringAssets;
	    ScopeGuard restoreAssets ([&] { this->m_registeringAssets = previousAssets; });
	    this->m_registeringAssets = nullptr;
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
    const bool previousUpdate = m_inUpdate;
    ScopeGuard restoreUpdate ([&] { m_inUpdate = previousUpdate; });
    m_inUpdate = previousUpdate || std::string_view (name) == "update";
    auto* previousAssets = this->m_registeringAssets;
    ScopeGuard restoreAssets ([&] { this->m_registeringAssets = previousAssets; });
    this->m_registeringAssets = nullptr;
    // check if there's an update method and run it
    JSValue function = JS_GetPropertyStr (this->m_context, module, name);
    ScopeGuard guard ([&] () { JS_FreeValue (this->m_context, function); });

    if (!JS_IsFunction (this->m_context, function)) {
	return JS_UNDEFINED;
    }

    return JS_Call (this->m_context, function, module, argc, argv);
}

void ScriptEngine::queueScript (const std::string& key, DynamicValue& currentValue, ScriptableObject& object, bool deferInitialization) {
    this->queueScript (key, currentValue, &object, deferInitialization);
}

void ScriptEngine::queueSceneScript (const std::string& property, const UserSetting& setting) {
    this->m_sceneObject->registerPropertyAnimation (property, setting);
    this->queueScript (property + "_scene", *setting.value, nullptr, false, property);
}

void ScriptEngine::queueScript (const std::string& key, DynamicValue& currentValue, ScriptableObject* object,
    bool deferInitialization, const std::string& sceneProperty) {
    const auto source = currentValue.getScriptSource ();

    static const bool traceScripts = std::getenv ("WPE_SCRIPT_TRACE") != nullptr;
    if (traceScripts) {
	sLog.out ("queueScript ", key, source.has_value () ? " HAS source" : " no source");
    }

    if (!source.has_value ()) {
	return;
    }

    auto it = this->m_scriptModules.find (key);

    if (it != this->m_scriptModules.end ()) {
	return;
    }

    // QuickJS module namespaces don't expose their exports as plain properties, so evaluating
    // the source as an ES6 module leaves `update` unreachable and property scripts inert.
    // Use the same strip-and-wrap approach as createLayerScript: remove the module syntax and
    // evaluate an IIFE that hands back the script's lifecycle hooks. The closure also captures
    // `thisLayer` per script instead of relying on a shared global that the last queued object
    // would otherwise overwrite.
    const auto [body, exportsWorkshopId] = normalizeSceneScriptModuleSyntax (*source);

    JSValue layer = object == nullptr ? JS_UNDEFINED : this->m_adapters.object->instantiate (*object);
    JS_SetPropertyStr (this->m_context, this->m_globalThis, "__propScriptLayer", layer);
    JS_SetPropertyStr (this->m_context, this->m_globalThis, "__propScriptSceneOwner",
        object == nullptr ? this->m_sceneObject->makePropertyOwner (sceneProperty) : JS_UNDEFINED);
    std::string animationScope;
    std::string propertyName;
    bool propertyAnimated = false;
    if (object != nullptr) {
	for (const auto& [name, property] : object->getProperties ()) {
	    if (property.key == key) {
		animationScope = property.animationScope;
		propertyName = name;
		propertyAnimated = property.setting != nullptr && property.setting->animation != nullptr;
		break;
	    }
	}
    }
    JS_SetPropertyStr (this->m_context, this->m_globalThis, "__propScriptPropertyAnimated",
	JS_NewBool (this->m_context, propertyAnimated));
    JS_SetPropertyStr (this->m_context, this->m_globalThis, "__propScriptPropertyName",
	JS_NewString (this->m_context, propertyName.c_str ()));
    JS_SetPropertyStr (this->m_context, this->m_globalThis, "__propScriptAnimationScope",
	JS_NewString (this->m_context, animationScope.c_str ()));

    // seed the user-property values bound to this setting so scriptProperties.X resolves to
    // the configured value (same seeding approach as createLayerScript)
    JSValue seedProps = JS_NewObject (this->m_context);

    for (const auto& [name, setting] : currentValue.getProperties ()) {
	JS_SetPropertyStr (this->m_context, seedProps, name.c_str (), this->dynamicToJs (*setting->value));
    }

    JS_SetPropertyStr (this->m_context, this->m_globalThis, "__propScriptProps", seedProps);

    std::ostringstream wrapper;
    wrapper << "(function() {\n"
	    // SceneScript property sources are ES modules, so their entire body is strict even
	    // when an authored `use strict` directive appears after generated/obfuscated setup
	    // code. Preserve that semantic after lowering the module into a classic IIFE. Besides
	    // matching Wallpaper Engine, this avoids legacy mapped `arguments` objects whose
	    // var-ref graph can be invalidated by those generated setup closures during QuickJS GC.
	    << "  'use strict';\n"
	    << "  var thisLayer = globalThis.__propScriptLayer;\n"
	    // Keep thisLayer on the owning layer. Model material scripts resolve
	    // registered uniform names and animations against their own pass.
	    << "  var __animationScope = globalThis.__propScriptAnimationScope;\n"
	    << "  var __propertyName = globalThis.__propScriptPropertyName;\n"
	    << "  var __modelMaterial = __animationScope.startsWith('material_');\n"
	    << "  function __materialKey(target, key) {\n"
	    << "    if (!__modelMaterial || typeof key !== 'string') return key;\n"
	    << "    var scoped = __animationScope + key;\n"
	    << "    return Reflect.get(target, scoped, target) === undefined ? key : scoped;\n"
	    << "  }\n"
	    << "  var thisObject = globalThis.__propScriptSceneOwner || ((__animationScope || globalThis.__propScriptPropertyAnimated || __propertyName.indexOf('_fx') >= 0) ? new Proxy(thisLayer, { get: function(target, key) {\n"
	    << "    if (key === 'getAnimation') return function(name) { return target.getAnimation(name === undefined ? __propertyName : __animationScope + name); };\n"
	    << "    var result = Reflect.get(target, __materialKey(target, key), target);\n"
	    << "    return typeof result === 'function' ? result.bind(target) : result;\n"
	    << "  }, set: function(target, key, value, receiver) {\n"
	    << "    var scoped = __materialKey(target, key);\n"
	    << "    return Reflect.set(target, scoped, value, scoped === key ? receiver : target);\n"
	    << "  } }) : thisLayer);\n"
	    // WE's scriptProperties builder doubles as the values object (scripts read
	    // scriptProperties.<name> without calling finish()), so the shim collects slider
	    // defaults straight onto the object it returns, seeded values winning
	    << "  var __props = Object.assign({}, globalThis.__propScriptProps || {});\n"
	    << "  function createScriptProperties() {\n"
	    << "    function reg(o){ if (o && o.name && !(o.name in __props)) __props[o.name] = o.value; return __props; }\n"
	    << "    __props.addSlider = reg; __props.addCheckbox = reg;\n"
	    << "    __props.addCombo = function(o){ var value = o.options[0].value;\n"
	    << "      if (!(o.name in __props)) __props[o.name] = value; return __props; };\n"
	    << "    __props.addColor = reg; __props.addText = reg; __props.addTextInput = reg;\n"
	    << "    __props.finish = function(){ return __props; };\n"
	    << "    return __props;\n"
	    << "  }\n"
	    << body << "\n"
	    << "  return {\n"
	    << "    update: (typeof update === 'function') ? update : null,\n"
	    << "    init: (typeof init === 'function') ? init : null,\n"
	    << "    destroy: (typeof destroy === 'function') ? destroy : null,\n"
	    << "    mediaPropertiesChanged: (typeof mediaPropertiesChanged === 'function') ? mediaPropertiesChanged : null,\n"
	    << "    mediaPlaybackChanged: (typeof mediaPlaybackChanged === 'function') ? mediaPlaybackChanged : null,\n"
	    << "    mediaTimelineChanged: (typeof mediaTimelineChanged === 'function') ? mediaTimelineChanged : null,\n"
	    << "    mediaThumbnailChanged: (typeof mediaThumbnailChanged === 'function') ? mediaThumbnailChanged : null,\n"
	    << "    applyUserProperties: (typeof applyUserProperties === 'function') ? applyUserProperties : null,\n"
	    << "    applyGeneralSettings: (typeof applyGeneralSettings === 'function') ? applyGeneralSettings : null,\n"
	    << "    resizeScreen: (typeof resizeScreen === 'function') ? resizeScreen : null,\n"
	    << "    cursorEnter: (typeof cursorEnter === 'function') ? cursorEnter : null,\n"
	    << "    cursorLeave: (typeof cursorLeave === 'function') ? cursorLeave : null,\n"
	    << "    cursorMove: (typeof cursorMove === 'function') ? cursorMove : null,\n"
	    << "    cursorDown: (typeof cursorDown === 'function') ? cursorDown : null,\n"
	    << "    cursorUp: (typeof cursorUp === 'function') ? cursorUp : null,\n"
	    << "    cursorClick: (typeof cursorClick === 'function') ? cursorClick : null,\n"
	    << "    animationEvent: (typeof animationEvent === 'function') ? animationEvent : null,\n"
	    // Resolve the exported binding in this module's closure when a hook calls createLayer.
	    << (exportsWorkshopId ? "    get __workshopId() { return __workshopId; },\n"
				  : "    __workshopId: null,\n")
	    << "  };\n"
	    << "})()";

    const std::string wrapped = wrapper.str ();
    std::map<std::string, bool> registeredAssets;
    JSValue module;
    {
	auto* previousAssets = this->m_registeringAssets;
	ScopeGuard restoreAssets ([&] { this->m_registeringAssets = previousAssets; });
	this->m_registeringAssets = &registeredAssets;
	module = JS_Eval (this->m_context, wrapped.c_str (), wrapped.size (), key.c_str (), JS_EVAL_TYPE_GLOBAL);
    }

    if (JS_IsException (module)) {
	logJSException (this->m_context, key.c_str ());
    }

    auto inserted = this->m_scriptModules.emplace (
	key,
	LoadedModule {
	    .value = currentValue,
	    .module = module,
	    .object = object,
	    .initialized = false,
	    .updateEnabled = true,
	    .animationEvents = body.find ("animationEvent") != std::string::npos,
	    .animationScope = animationScope,
	    .cursorEvents = body.find ("cursorEnter") != std::string::npos
		|| body.find ("cursorLeave") != std::string::npos || body.find ("cursorMove") != std::string::npos
		|| body.find ("cursorDown") != std::string::npos || body.find ("cursorUp") != std::string::npos
		|| body.find ("cursorClick") != std::string::npos,
	    .registeredAssets = std::move (registeredAssets),
	}
    );

    if (!inserted.second) {
	return;
    }
    this->m_scriptModuleOrder.emplace_back (&inserted.first->first, &inserted.first->second);

    // Constructor registrations must wait until the complete renderer is in the scene.
    if (!deferInitialization && this->m_sceneLayersReady
        && (object == nullptr || this->m_scene.getObject (object->getId ()) == object)) {
	this->initializeModule (key, inserted.first->second);
    }
}

void ScriptEngine::callLifecycleHook (const std::string& key, LoadedModule& loaded, const char* hook) {
    auto* previousModule = this->m_runningModule;
    ScopeGuard restoreModule ([&] { this->m_runningModule = previousModule; });
    this->m_runningModule = &loaded;

    const bool update = std::string_view (hook) == "update";
    auto* model = update ? dynamic_cast<Render::Objects::CModel*> (loaded.object) : nullptr;
    auto* blend = model != nullptr ? model->findAnimatedBlend (loaded.value) : nullptr;
    const double time = this->m_scene.getElapsedTime ();
    const bool angles = isAnglesProperty (key);
    const glm::vec3 initialDegrees = angles ? glm::degrees (loaded.value.getVec3 ()) : glm::vec3 (0.0f);
    // Scripts retain init/update arguments as base values. They must not follow
    // later property writes, or audio scale multipliers compound every frame.
    JSValue args[] = { angles ? anglesToJs (*this->m_adapters.vec3, loaded.value)
	: blend != nullptr ? JS_NewFloat64 (this->m_context, blend->settings->blend->evaluateFloat (time))
	: this->dynamicToJs (loaded.value, true) };
    JSValue result = this->call (loaded.module, 1, args, hook);

    ScopeGuard guard ([this, args, result] () {
	JS_FreeValue (this->m_context, result);
	JS_FreeValue (this->m_context, args[0]);
    });

    if (JS_IsException (result)) {
	logJSException (this->m_context, key.c_str ());
	if (update) loaded.updateEnabled = false;
	return;
    }

    if (angles) {
	jsToAngles (this->m_context, result, args[0], loaded.value, initialDegrees);
    } else if (blend != nullptr) {
	// A returned weight overrides this pose sample without changing the curve's base value.
	if (JS_IsNumber (result)) {
	    double value = 0.0;
	    JS_ToFloat64 (this->m_context, &value, result);
	    const float weight = static_cast<float> (value);
	    if (std::isfinite (weight)) model->applyScriptBlend (*blend, weight, time);
	}
    } else {
	// Native layer visibility keeps its current value when update() returns a number.
	if (update && JS_IsNumber (result) && loaded.object != nullptr && key.starts_with ("visible_")) {
	    const auto& properties = loaded.object->getProperties ();
	    const auto visibility = properties.find ("visible");
	    if (visibility != properties.end () && visibility->second.key == key) return;
	}
	jsToDynamicValue (this->m_context, result, loaded.value);
    }
}

void ScriptEngine::initializeModule (const std::string& key, LoadedModule& loaded) {
    if (loaded.initialized) {
	return;
    }

    loaded.initialized = true;
    loaded.lastOutputSize = this->m_scene.getOutputSize ();

    // run the script's init hook (if any) followed by the first update, mirroring WE's lifecycle
    this->callLifecycleHook (key, loaded, "init");
    this->notifyMediaUpdate (this->m_mediaSource.getMediaInfo (), &loaded);
    this->callLifecycleHook (key, loaded, "update");
}

void ScriptEngine::unregisterScriptable (const ScriptableObject* object) {
    if (object == nullptr) {
	return;
    }

    this->m_adapters.object->invalidate (object);

    std::erase_if (this->m_scriptModuleOrder, [object] (const auto& entry) {
	return entry.second->object == object;
    });
    for (auto it = this->m_scriptModules.begin (); it != this->m_scriptModules.end ();) {
	if (it->second.object != object) {
	    ++it;
	    continue;
	}

	// A hook that is running right now can only be running against this object if the object is
	// being destroyed from inside its own script, which the engine never does; clear the pointer
	// anyway so a later getRunningModule() cannot hand out the entry that is about to be erased.
	if (this->m_runningModule == &it->second) {
	    this->m_runningModule = nullptr;
	}

	JS_FreeValue (this->m_context, it->second.module);
	it = this->m_scriptModules.erase (it);
    }
}

void ScriptEngine::unregisterPropertyScript (const std::string& key, const ScriptableObject* object) {
    const auto found = m_scriptModules.find (key);
    if (found == m_scriptModules.end () || found->second.object != object) return;
    std::erase_if (m_scriptModuleOrder, [&] (const auto& entry) { return entry.second == &found->second; });
    JS_FreeValue (m_context, found->second.module);
    m_scriptModules.erase (found);
}

void ScriptEngine::initializeQueuedScripts (ScriptableObject* target) {
    this->m_sceneLayersReady = true;

    // Every init() has to run before any first update(). Scripts publish cross-layer state onto the
    // `shared` global from init() and consume it from update(), so interleaving the two makes an
    // early layer's first update throw on state a later layer has not created yet — and a throw
    // there disables that script's update permanently, for the rest of the scene's life.
    std::vector<std::pair<const std::string*, LoadedModule*>> started;

    {
	const bool previous = m_initializingScripts;
	ScopeGuard restoreInitialization ([&] { m_initializingScripts = previous; });
	m_initializingScripts = true;
	for (const auto& entry : this->m_scriptModuleOrder) {
	    const auto& key = *entry.first;
	    auto& module = *entry.second;
	    if (module.initialized || (target != nullptr && module.object != target)) {
		continue;
	    }

	    module.initialized = true;
	    module.lastOutputSize = this->m_scene.getOutputSize ();
	    started.emplace_back (&key, &module);
	    this->callLifecycleHook (key, module, "init");
	    this->m_scene.loadingCheckpoint ();
	}

	// Wallpaper Engine hands the scripts their user properties once, after every init() and before
	// any first update(). Scripts gate their behaviour on that call rather than on defaults, so
	// skipping it strands them in their inert branch - Passing Breeze 2244339517 only starts
	// orbiting its camera because applyUserProperties sets isCircular.
	if (target == nullptr) {
	    this->dispatchAllUserProperties ();
	} else {
	    for (const auto& [key, module] : started) this->dispatchAllUserProperties (module);
	}
    }
    // Children created by init wait for the outer startup broadcast. Children
    // created later by update receive no replay, matching the native lifecycle.
    this->dispatchGeneralSettings ();

    // A player may already be running before these modules subscribe. Seed only
    // the new modules after every init, so handlers can safely access other layers.
    for (const auto& [key, module] : started) {
	this->notifyMediaUpdate (this->m_mediaSource.getMediaInfo (), module);
    }

    // createLayer initializes immediately, but its first update belongs to the
    // normal traversal. Startup also includes children created by init hooks,
    // after every existing layer has finished initializing.
    if (target == nullptr) {
	for (const auto& [key, module] : this->m_scriptModuleOrder) {
	    if (module->initialized && module->updateEnabled)
		this->callLifecycleHook (*key, *module, "update");
	    this->m_scene.loadingCheckpoint ();
	}
    }
}

void ScriptEngine::applyUserProperties (JSValue changed, LoadedModule* target) {
    // property-script wrappers capture their own thisLayer in the eval closure, so no
    // per-module global rebinding is needed here (unlike beingsuz's original)
    for (auto& module : this->m_scriptModules | std::views::values) {
	if (target != nullptr && target != &module) continue;
	auto* previousModule = this->m_runningModule;
	ScopeGuard restoreModule ([&] { this->m_runningModule = previousModule; });
	this->m_runningModule = &module;
	JSValue args[] = { changed };
	JSValue result = this->call (module.module, 1, args, "applyUserProperties");

	if (JS_IsException (result)) {
	    logJSException (this->m_context, "applyUserProperties");
	}

	JS_FreeValue (this->m_context, result);
    }
}

void ScriptEngine::dispatchGeneralSettings () {
    if (m_context == nullptr || m_initializingScripts) return;
    const auto language = m_scene.getContext ().getApp ().getContext ().getLanguage ();
    if (language == m_lastGeneralLanguage) return;
    m_lastGeneralLanguage = language;

    // The list remains stable if a callback appends a layer. Native includes
    // those new modules in this broadcast, giving each its own mutable payload.
    for (const auto& [key, module] : m_scriptModuleOrder) {
        if (!module->initialized) continue;
        auto* previous = m_runningModule;
        ScopeGuard restoreModule ([&] { m_runningModule = previous; });
        m_runningModule = module;
        JSValue settings = JS_NewObject (m_context);
        JS_SetPropertyStr (m_context, settings, "language", JS_NewString (m_context, language.c_str ()));
        JSValue result = this->call (module->module, 1, &settings, "applyGeneralSettings");
        if (JS_IsException (result)) logJSException (m_context, "applyGeneralSettings");
        JS_FreeValue (m_context, result);
        JS_FreeValue (m_context, settings);
    }
}

void ScriptEngine::dispatchUserProperty (const std::string& key, DynamicValue& value) {
    if (this->m_context == nullptr) {
	return;
    }

    JSValue changed = JS_NewObject (this->m_context);
    JS_SetPropertyStr (this->m_context, changed, key.c_str (), this->dynamicToJs (value));
    this->applyUserProperties (changed);
    JS_FreeValue (this->m_context, changed);
}

void ScriptEngine::dispatchAllUserProperties (LoadedModule* target) {
    if (this->m_context == nullptr) {
	return;
    }

    // Scene::project is a reference member, so the properties stay mutable through the const
    // getter - dynamicToJs needs a non-const DynamicValue&.
    auto& properties = this->m_scene.getScene ().project.properties;
    JSValue all = JS_NewObject (this->m_context);
    for (auto& [key, value] : properties) {
	if (value != nullptr) {
	    JS_SetPropertyStr (this->m_context, all, key.c_str (), this->dynamicToJs (*value));
	}
    }
    this->applyUserProperties (all, target);
    JS_FreeValue (this->m_context, all);
}

std::string ScriptEngine::getRunningModuleWorkshopId () const {
    if (this->m_runningModule == nullptr || this->m_context == nullptr) {
	return "";
    }

    JSValue wid = JS_GetPropertyStr (this->m_context, this->m_runningModule->module, "__workshopId");
    std::string result;

    if (JS_IsString (wid)) {
	size_t length = 0;
	const char* str = JS_ToCStringLen (this->m_context, &length, wid);
	if (str != nullptr) {
	    const std::string_view id (str, length);
	    // The workshop id is one numeric directory component in the asset path.
	    if (id.find_first_not_of ("0123456789") == std::string_view::npos) {
		result = id;
	    }
	    JS_FreeCString (this->m_context, str);
	}
    }

    JS_FreeValue (this->m_context, wid);
    return result;
}

void ScriptEngine::tick () {
    this->dispatchGeneralSettings ();
    this->dispatchScreenResize ();

    // run intervals
    this->m_engineObject->tick ();
    this->m_scene.loadingCheckpoint ();

    // Dispatch layer cursor events before update() so click handlers that change
    // shared state are visible to property scripts in the same frame.
    this->dispatchCursorEvents ();

    // A layer can have several property scripts. Advance each named timeline once
    // and deliver its authored markers to all animationEvent hooks on that layer.
    std::unordered_set<ScriptableObject*> animatedObjects;
    for (const auto& [key, module] : m_scriptModules) {
	if (module.initialized && module.object != nullptr) animatedObjects.insert (module.object);
    }
    for (auto* object : m_scene.getObjectsByRenderOrder ()) {
	if (auto* scriptable = dynamic_cast<ScriptableObject*> (object)) animatedObjects.insert (scriptable);
    }
    for (auto* object : animatedObjects) object->prepareAnimationEvents ();
    for (auto* object : animatedObjects) {
	for (const auto& [name, animation] : object->getAnimations ()) {
	    for (const auto& event : animation->takeEvents (m_scene.getElapsedTime ())) {
		std::string scope;
		for (const auto& [propertyName, property] : object->getProperties ()) {
		    if (property.setting != nullptr && property.setting->animation.get () == animation) {
			scope = property.animationScope;
			break;
		    }
		}
		dispatchAnimationEvent (*object, event, animation->name.empty () ? name : animation->name, scope);
	    }
	}
    }

    // Run all update methods through the same property path as the first update.
    for (const auto& entry : this->m_scriptModuleOrder) {
	const auto& key = *entry.first;
	auto& module = *entry.second;
	if (!module.initialized || !module.updateEnabled) {
	    continue;
	}
	this->callLifecycleHook (key, module, "update");
	if (module.updateEnabled) this->m_scene.loadingCheckpoint ();
    }
}

void ScriptEngine::dispatchScreenResize () {
    const glm::vec2 outputSize = this->m_scene.getOutputSize ();
    for (const auto& [key, module] : this->m_scriptModuleOrder) {
	if (!module->initialized || module->lastOutputSize == outputSize) continue;
	// Each module starts at its initialization size. A layer created by this
	// callback must not receive the resize that created it as a startup event.
	module->lastOutputSize = outputSize;
	auto* previous = this->m_runningModule;
	ScopeGuard restore ([&] { this->m_runningModule = previous; });
	this->m_runningModule = module;
	DynamicValue size (outputSize);
	JSValue args[] = { this->m_adapters.vec2->instantiate (size, true) };
	JSValue result = this->call (module->module, 1, args, "resizeScreen");
	if (JS_IsException (result)) {
	    logJSException (this->m_context, key->c_str ());
	}
	JS_FreeValue (this->m_context, result);
	JS_FreeValue (this->m_context, args[0]);
    }
}

void ScriptEngine::dispatchAnimationEvent (
    ScriptableObject& object, const PropertyAnimation::Event& event, const std::string& animationName,
    const std::string& animationScope
) {
    auto* previousModule = m_runningModule;
    ScopeGuard restore ([&] { m_runningModule = previousModule; });
    for (auto& [key, module] : m_scriptModules) {
	if (!module.initialized || !module.animationEvents || module.object != &object) continue;
	if (!module.animationScope.empty () && module.animationScope != animationScope) continue;
	m_runningModule = &module;
	JSValue eventValue = JS_NewObject (m_context);
	JS_SetPropertyStr (m_context, eventValue, "name", JS_NewString (m_context, event.name.c_str ()));
	JS_SetPropertyStr (m_context, eventValue, "frame", JS_NewFloat64 (m_context, event.frame));
	JS_SetPropertyStr (m_context, eventValue, "animation", JS_NewString (m_context, animationName.c_str ()));
	const bool angles = isAnglesProperty (key);
	const glm::vec3 initialDegrees = angles ? glm::degrees (module.value.getVec3 ()) : glm::vec3 (0.0f);
	JSValue args[] = { eventValue, angles ? anglesToJs (*m_adapters.vec3, module.value)
	    : dynamicToJs (module.value, true) };
	JSValue result = call (module.module, 2, args, "animationEvent");
	if (JS_IsException (result)) {
	    logJSException (m_context, key.c_str ());
	    module.animationEvents = false;
	} else if (angles) {
	    jsToAngles (m_context, result, args[1], module.value, initialDegrees);
	} else {
	    jsToDynamicValue (m_context, result, module.value);
	}
	JS_FreeValue (m_context, result);
	JS_FreeValue (m_context, args[1]);
	JS_FreeValue (m_context, eventValue);
    }
}

JSValue ScriptEngine::makeCursorEvent (
    const glm::vec3& worldPosition, const glm::vec3& localPosition, const std::optional<std::string>& hitBox
) {
    DynamicValue world (worldPosition);
    DynamicValue local (localPosition);
    JSValue event = JS_NewObject (this->m_context);
    JS_SetPropertyStr (this->m_context, event, "worldPosition", this->m_adapters.vec3->instantiate (world, true));
    JS_SetPropertyStr (this->m_context, event, "localPosition", this->m_adapters.vec3->instantiate (local, true));
    if (hitBox) JS_SetPropertyStr (m_context, event, "hitBox", JS_NewString (m_context, hitBox->c_str ()));
    return event;
}

void ScriptEngine::dispatchCursorEvents () {
    const auto& input = this->m_scene.getContext ().getInputContext ().getMouseInput ();
    const bool leftDown = input.leftClick () == Input::MouseClickStatus::Clicked;
    const glm::vec3 worldPosition
	= this->m_scene.getCamera ().screenToWorld (*this->m_scene.getMousePositionNormalized ());
    const glm::vec3 cursorDelta
	= worldPosition - this->m_lastCursorWorldPosition.value_or (worldPosition + glm::vec3 (1.0f));
    const bool moved = !this->m_lastCursorWorldPosition.has_value ()
	|| glm::dot (cursorDelta, cursorDelta) > 1e-8f;
    // Native only resolves bone boxes on button transitions or while a press
    // that began inside a layer remains active; ordinary hover omits hitBox.
    const bool selectHitBox = leftDown != m_cursorLeftDown
	|| std::ranges::any_of (m_scriptModules, [] (const auto& entry) { return entry.second.cursorPressedInside; });

    using CursorModule = std::pair<const std::string*, LoadedModule*>;
    std::unordered_map<Render::Objects::CImage*, std::vector<CursorModule>> modules;
    for (auto& [key, module] : m_scriptModules) {
	if (!module.initialized || !module.cursorEvents) continue;
	if (auto* image = dynamic_cast<Render::Objects::CImage*> (module.object))
	    modules[image].emplace_back (&key, &module);
    }

    // Both snapshots remain stable if a callback creates, reorders or queues
    // removal of a layer. Native visits frontmost layers first, including
    // scriptless blockers, and broadcasts each event to all modules on a layer.
    const auto order = m_scene.getObjectsByCursorOrder ();
    for (auto* object : order | std::views::reverse) {
	auto* image = dynamic_cast<Render::Objects::CImage*> (object);
	if (!image) continue;
	const auto found = modules.find (image);
	if (found == modules.end () && !image->getObject ().disablePropagation) continue;
	const auto localPosition = image->cursorLocalPosition (worldPosition);
	const bool inside = localPosition.has_value ();
	const auto hitBox = selectHitBox ? image->cursorHitBox (worldPosition) : std::nullopt;

	if (found != modules.end ()) {
	    const auto callCursorHook = [&] (const char* hook, auto accepts) {
		for (const auto& [key, module] : found->second) {
		    if (!accepts (*module)) continue;
		    auto* previous = m_runningModule;
		    ScopeGuard restoreModule ([&] { m_runningModule = previous; });
		    m_runningModule = module;
		    const glm::vec3 local = localPosition.value_or (glm::vec3 (0.0f));
		    const glm::vec3 hit = inside && !m_scene.getCamera ().isOrthogonal ()
			? glm::vec3 (image->resolveWorldMatrix () * glm::vec4 (local, 1.0f)) : worldPosition;
		    JSValue event = makeCursorEvent (hit, local, hitBox);
		    JSValue result = call (module->module, 1, &event, hook);
		    if (JS_IsException (result)) logJSException (m_context, key->c_str ());
		    JS_FreeValue (m_context, result);
		    JS_FreeValue (m_context, event);
		}
	    };
	    const auto all = [] (const LoadedModule&) { return true; };
	    if (inside) {
		callCursorHook ("cursorEnter", [] (const LoadedModule& m) { return !m.cursorInside; });
		if (moved) callCursorHook ("cursorMove", all);
		if (leftDown && !m_cursorLeftDown) {
		    for (const auto& [key, module] : found->second) module->cursorPressedInside = true;
		    callCursorHook ("cursorDown", all);
		}
		if (!leftDown && m_cursorLeftDown) {
		    callCursorHook ("cursorUp", all);
		    callCursorHook ("cursorClick", [] (const LoadedModule& m) { return m.cursorPressedInside; });
		}
	    } else {
		callCursorHook ("cursorLeave", [] (const LoadedModule& m) { return m.cursorInside; });
	    }
	    for (const auto& [key, module] : found->second) {
		module->cursorInside = inside;
	    }
	}

	// Visibility changes in the callbacks apply immediately. Covered layers
	// retain their prior hover state until propagation reaches them again.
	if (inside && image->getObject ().disablePropagation
	    && image->getImage ().visible->value->getBool () && image->isVisibleThroughParents ()) break;
    }

    // A covered release still ends the press. Retaining it would turn a later
    // outside press and inside release into a spurious click on the lower layer.
    if (!leftDown && m_cursorLeftDown) {
	for (auto& [key, module] : m_scriptModules) module.cursorPressedInside = false;
    }
    this->m_cursorLeftDown = leftDown;
    this->m_lastCursorWorldPosition = worldPosition;
}

void ScriptEngine::finishLoading (std::chrono::steady_clock::duration timerOffset) {
    this->m_engineObject->rebaseTimers (timerOffset);
    this->notifyMediaUpdate (this->m_mediaSource.getMediaInfo ());
}

void ScriptEngine::notifyMediaUpdate (const Media::MediaSource::MediaInfo& media, LoadedModule* target) {
    // Initialization may explicitly seed one complete module; global media events
    // from outgoing frames must never reenter a partially constructed candidate.
    if (this->m_scene.isLoading () && target == nullptr) return;
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

    JSValue propertiesEvent = JS_NewObject (ctx);

    // set properties
    JS_SetPropertyStr (ctx, propertiesEvent, "title", JS_NewString (ctx, media.title.c_str ()));
    JS_SetPropertyStr (ctx, propertiesEvent, "artist", JS_NewString (ctx, media.artist.c_str ()));
    JS_SetPropertyStr (ctx, propertiesEvent, "albumTitle", JS_NewString (ctx, media.album.c_str ()));

    JSValue playbackEvent = JS_NewObject (ctx);

    JS_SetPropertyStr (ctx, playbackEvent, "state", JS_NewInt32 (ctx, media.playbackState));

    JSValue mediaTimelineEvent = JS_NewObject (ctx);

    // MPRIS stores microseconds; SceneScript exposes timeline values in seconds.
    JS_SetPropertyStr (ctx, mediaTimelineEvent, "position", JS_NewFloat64 (ctx, media.position / 1000000.0));
    JS_SetPropertyStr (ctx, mediaTimelineEvent, "duration", JS_NewFloat64 (ctx, media.duration / 1000000.0));

    JSValue mediaThumbnailEvent = JS_NewObject (ctx);

    JS_SetPropertyStr (ctx, mediaThumbnailEvent, "hasThumbnail", JS_NewBool (ctx, media.url.has_value ()));
    JS_SetPropertyStr (ctx, mediaThumbnailEvent, "primaryColor", primaryColor);
    JS_SetPropertyStr (ctx, mediaThumbnailEvent, "secondaryColor", secondaryColor);
    JS_SetPropertyStr (ctx, mediaThumbnailEvent, "tertiaryColor", tertiaryColor);
    JS_SetPropertyStr (ctx, mediaThumbnailEvent, "highContrastColor", highContrastColor);
    // Until cover palette extraction is implemented, use the contrast color of
    // the fallback palette. Consumers still require a usable Vec3 text color.
    JS_SetPropertyStr (
	ctx, mediaThumbnailEvent, "textColor", this->m_adapters.vec3->instantiate (highContrastColorValue, true)
    );

    JSValue propertiesArgs[] = { propertiesEvent };
    JSValue playbackArgs[] = { playbackEvent };
    JSValue mediaTimelineArgs[] = { mediaTimelineEvent };
    JSValue mediaThumbnailArgs[] = { mediaThumbnailEvent };

    for (auto& module : this->m_scriptModules | std::views::values) {
	if (!module.initialized || (target != nullptr && target != &module)) {
	    continue;
	}
	auto* previous = this->m_runningModule;
	ScopeGuard restore ([&] { this->m_runningModule = previous; });
	this->m_runningModule = &module;
	const auto dispatch = [&] (const char* name, JSValue* args) {
	    JSValue result = this->call (module.module, 1, args, name);
	    if (JS_IsException (result)) {
		logJSException (ctx, name);
	    }
	    JS_FreeValue (ctx, result);
	};
	// Newly initialized modules intentionally receive the complete current state.
	// Later position polls must not restart cover transitions or playback handlers.
	const auto& last = module.lastMedia;
	const bool initial = target != nullptr || !last.has_value ();
	const bool propertiesChanged = initial || media.title != last->title
	    || media.artist != last->artist || media.album != last->album;
	const bool playbackChanged = initial || media.playbackState != last->playbackState;
	const bool timelineChanged = initial || media.position != last->position || media.duration != last->duration;
	const bool thumbnailChanged = initial || media.url != last->url;
	module.lastMedia = media;
	if (propertiesChanged) {
	    dispatch ("mediaPropertiesChanged", propertiesArgs);
	}
	if (playbackChanged) {
	    dispatch ("mediaPlaybackChanged", playbackArgs);
	}
	if (timelineChanged) {
	    dispatch ("mediaTimelineChanged", mediaTimelineArgs);
	}
	if (thumbnailChanged) {
	    dispatch ("mediaThumbnailChanged", mediaThumbnailArgs);
	}
    }

    // free all created objects as we don't keep a ref to them anymore
    JS_FreeValue (ctx, propertiesEvent);
    JS_FreeValue (ctx, playbackEvent);
    JS_FreeValue (ctx, mediaTimelineEvent);
    JS_FreeValue (ctx, mediaThumbnailEvent);
}
