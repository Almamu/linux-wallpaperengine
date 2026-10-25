#pragma once

#include "Adapters/VectorAdapter.h"
#include "ConsoleObject.h"
#include "EngineObject.h"
#include "InputObject.h"
#include "Modules/ScriptModule.h"
#include "SceneObject.h"

#include <chrono>
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "WallpaperEngine/Data/Model/DynamicValue.h"
#include "WallpaperEngine/Data/Model/Types.h"
#include "WallpaperEngine/Media/MediaSource.h"

namespace WallpaperEngine::Media {
class MediaSource;
}
extern "C" {
#include "quickjs.h"
}

namespace WallpaperEngine::Render::Wallpapers {
class CScene;
}

namespace WallpaperEngine::Scripting {
class ScriptPropertiesObject;
namespace Adapters {
    class ScriptableObjectAdapter;
}
using namespace WallpaperEngine::Data::Model;

// Opaque handle returned by createLayerScript. 0 means invalid / not created.
using ScriptLayerHandle = int;
static constexpr ScriptLayerHandle kInvalidLayerHandle = 0;

class ScriptEngine {
public:
    /**
     * A property script (e.g. a scripted "origin") loaded as an ES module.
     *
     * `module` is the module namespace (holds the exports such as init/update),
     * or JS_UNDEFINED when loading failed. `thisLayer` is the ILayer object the
     * script sees as the global `thisLayer` while any of its functions run.
     */
    struct LoadedModule {
	DynamicValue& value;
	JSValue module;
	JSValue thisLayer;
	bool initialized = false;
	bool errorLogged = false;
    };
    struct JSObjectAdapters {
	std::unique_ptr<Adapters::VectorAdapter<4>> vec4;
	std::unique_ptr<Adapters::VectorAdapter<3>> vec3;
	std::unique_ptr<Adapters::VectorAdapter<2>> vec2;
	std::unique_ptr<Adapters::ScriptableObjectAdapter> object;
    };

    ~ScriptEngine ();
    ScriptEngine (Render::Wallpapers::CScene& scene, Media::MediaSource& mediaSource);
    ScriptEngine (const ScriptEngine&) = delete;
    ScriptEngine& operator= (const ScriptEngine&) = delete;

    JSRuntime* getRuntime () const { return m_runtime; }
    JSContext* getContext () const { return m_context; }
    JSValue getGlobalThis () const { return m_globalThis; }
    LoadedModule* getRunningModule () const { return m_runningModule; }
    JSValue dynamicToJs (DynamicValue& value) const;
    /**
     * Like dynamicToJs, but vectors are detached copies instead of live
     * bindings, so scripts can modify them without touching engine state
     * (Wallpaper Engine hands scripts copies, e.g. `thisLayer.size`).
     */
    JSValue dynamicToJsCopy (DynamicValue& value) const;
    /**
     * Stores a script value (number, bool, string or {x, y[, z[, w]]} object) into `target`.
     * undefined leaves `target` untouched, null clears it.
     */
    static void jsToDynamicValue (JSContext* ctx, JSValue val, DynamicValue& target);

    /**
     * Load a property script (ES module) for the given object property.
     *
     * The module's top level runs immediately. Its `init(value)` export runs
     * on the first tick() and `update(value)` on every tick(); a return value
     * from either replaces the property value.
     *
     * @param key Unique module key (property name + object id)
     * @param currentValue The property value the script drives
     * @param object The object exposed to the script as `thisLayer`
     */
    void queueScript (const std::string& key, DynamicValue& currentValue, ScriptableObject& object);

    /**
     * Runs a frame tick in the javascript engine. Dispatches any pending events,
     * timeouts, intervals AND calls any init()/update() functions.
     */
    void tick ();

    // -------------------------------------------------------------------
    // Layer-script API (Phase 2 — dynamic text)
    // -------------------------------------------------------------------
    //
    // Wallpaper Engine text-object scripts follow a lifecycle pattern that
    // cannot be evaluated with the simple `update(value) -> value` contract
    // above. They typically look like:
    //
    //   'use strict';
    //   export var scriptProperties = createScriptProperties()…finish();
    //   export function init()   { /* subscribe to events, cache data    */ }
    //   export function update() { thisLayer.text = computeCurrentText(); }
    //
    // The script mutates a `thisLayer` object in place instead of returning
    // a value, and lifecycle functions are optional. The API below keeps
    // per-layer state alive across frames so `init()` runs once and
    // `update()` re-runs every tick.

    /**
     * Create a persistent "layer script" from a WE text-object script.
     *
     * @param scriptSource The full JS script text.
     * @param initialScriptProps Initial values for scriptProperties entries.
     *        Ownership stays with the caller; we only snapshot current values.
     * @param initialText Initial value of `thisLayer.text` (usually the
     *        static placeholder carried in the JSON).
     * @return A positive handle, or kInvalidLayerHandle if evaluation failed.
     */
    ScriptLayerHandle createLayerScript (
	const std::string& scriptSource, std::map<std::string, UserSettingUniquePtr>& initialScriptProps,
	const std::string& initialText
    );

    /**
     * Advance a layer by one frame.
     *
     * On the first call, invokes `init()` (if defined) before `update()`.
     * Updates a `thisScene` context visible to the script (time, fps).
     * Silently no-ops if the handle is invalid.
     */
    void tickLayer (ScriptLayerHandle handle, double time, double deltaTime, double fps);

    /**
     * Read the current value of `thisLayer.text` for the given layer.
     * Returns an empty string if the handle is invalid.
     */
    std::string layerText (ScriptLayerHandle handle);

    /**
     * Tear down a layer: invokes `destroy()` (if defined) and frees state.
     */
    void destroyLayer (ScriptLayerHandle handle);

    const JSObjectAdapters& getAdapters () const { return m_adapters; }
    const Render::Wallpapers::CScene& getScene () const { return m_scene; }
    const std::map<std::string, std::unique_ptr<Modules::ScriptModule>>& getModules () const { return m_modules; }

private:
    JSValue call (JSValue module, int argc, JSValueConst argv[], const char* name);
    /** Calls `name` on a property script with a copy of its value and applies the result */
    void runModuleFunction (const std::string& key, LoadedModule& module, const char* name);
    /** Logs the pending JS exception once per module so per-frame errors don't spam */
    void logModuleException (const std::string& key, LoadedModule& module, const char* stage);

    void installBuiltins ();

    void notifyMediaUpdate (const Media::MediaSource::MediaInfo& media);

    // Installs globalThis.__layers and related helpers. Called lazily.
    void ensureLayerRegistry ();

    JSRuntime* m_runtime = nullptr;
    JSContext* m_context = nullptr;
    JSValue m_globalThis;
    Render::Wallpapers::CScene& m_scene;
    std::unique_ptr<EngineObject> m_engineObject;
    std::unique_ptr<InputObject> m_inputObject;
    std::unique_ptr<SceneObject> m_sceneObject;
    std::unique_ptr<ConsoleObject> m_consoleObject;
    std::unique_ptr<ScriptPropertiesObject> m_scriptPropertiesObject;

    std::map<std::string, std::unique_ptr<Modules::ScriptModule>> m_modules = {};
    std::map<std::string, LoadedModule> m_scriptModules = {};

    LoadedModule* m_runningModule = nullptr;

    ScriptLayerHandle m_nextLayerId = 1;
    bool m_layerRegistryReady = false;
    std::map<ScriptLayerHandle, bool> m_layerInitialized;
    bool m_builtinsInstalled = false;
    Media::MediaSource& m_mediaSource;
    std::function<void ()> m_unregisterMediaUpdateCallback;
    std::function<void ()> m_unregisterAlbumArtUpdateCallback;

    JSObjectAdapters m_adapters;
};
} // namespace WallpaperEngine::Scripting
