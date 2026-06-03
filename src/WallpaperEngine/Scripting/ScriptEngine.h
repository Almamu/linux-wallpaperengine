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

extern "C" {
#include "quickjs.h"
}

namespace WallpaperEngine::Render::Wallpapers {
class CScene;
}

namespace WallpaperEngine::Scripting {
namespace Adapters {
    class ScriptableObjectAdapter;
}
using namespace WallpaperEngine::Data::Model;

// Opaque handle returned by createLayerScript. 0 means invalid / not created.
using ScriptLayerHandle = int;
static constexpr ScriptLayerHandle kInvalidLayerHandle = 0;

class ScriptEngine {
public:
    struct JSObjectAdapters {
	std::unique_ptr<Adapters::VectorAdapter<4>> vec4;
	std::unique_ptr<Adapters::VectorAdapter<3>> vec3;
	std::unique_ptr<Adapters::VectorAdapter<2>> vec2;
	std::unique_ptr<Adapters::ScriptableObjectAdapter> object;
    };

    ~ScriptEngine ();
    ScriptEngine (Render::Wallpapers::CScene& scene);
    ScriptEngine (const ScriptEngine&) = delete;
    ScriptEngine& operator= (const ScriptEngine&) = delete;

    JSRuntime* getRuntime () const { return m_runtime; }
    JSContext* getContext () const { return m_context; }

    /**
     * Evaluate a WallpaperEngine script's update() function.
     *
     * @param key The full JS script text (ES6 module with export function update(value))
     * @param currentValue The current value to pass to update()
     * @return The modified value from update(), or a copy of currentValue on error
     */
    void queueScript (const std::string& key, DynamicValue& currentValue, ScriptableObject& object);

    /**
     * Runs a frame tick in the javascript engine. Dispatches any pending events,
     * timeouts, intervals AND calls any update() functions.
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
	const std::string& scriptSource, std::map<std::string, DynamicValue>& initialScriptProps,
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
    struct LoadedModule {
	DynamicValue& value;
	JSValue module;
    };

    JSValue call (JSValue module, int argc, JSValueConst argv[], const char* name);

    void installBuiltins ();
    void refreshMediaState ();
    void dispatchMediaEvents (JSValue module, const void* bindingKey);
    void callModuleWithProps (JSContext* ctx, JSValue module, const char* name, JSValue propsObj) const;
    void runIntervals (JSContext* ctx, JSValue globalObj, const std::string& bindingKeyString) const;
    DynamicValueUniquePtr fallbackTextValue (const std::string& scriptSource) const;
    void applyTextFallback (DynamicValue& value, const std::string& scriptSource) const;

    JSValue dynamicToJs (DynamicValue& value) const;

    struct MediaState {
	int playbackState = 0;
	std::string status = "Stopped";
	std::string title;
	std::string artist;
	std::string artUrl;
	double duration = 0.0;
	double position = 0.0;
	bool available = false;
    };

    // Installs globalThis.__layers and related helpers. Called lazily.
    void ensureLayerRegistry ();

    JSRuntime* m_runtime = nullptr;
    JSContext* m_context = nullptr;
    JSValue m_globalThis;
    JSValue m_scriptProps;
    Render::Wallpapers::CScene& m_scene;
    std::unique_ptr<EngineObject> m_engineObject;
    std::unique_ptr<InputObject> m_inputObject;
    std::unique_ptr<SceneObject> m_sceneObject;
    std::unique_ptr<ConsoleObject> m_consoleObject;

    std::map<std::string, std::unique_ptr<Modules::ScriptModule>> m_modules = {};
    std::map<std::string, LoadedModule> m_scriptModules = {};

    ScriptLayerHandle m_nextLayerId = 1;
    bool m_layerRegistryReady = false;
    std::map<ScriptLayerHandle, bool> m_layerInitialized;
    std::unordered_set<const void*> m_initializedModules = {};
    std::unordered_map<const void*, std::string> m_lastMediaProperties = {};
    std::unordered_map<const void*, std::string> m_lastMediaPlayback = {};
    std::unordered_map<const void*, std::string> m_lastMediaTimeline = {};
    std::unordered_map<const void*, std::string> m_lastMediaThumbnail = {};
    std::chrono::steady_clock::time_point m_lastMediaPoll = {};
    std::future<std::optional<MediaState>> m_mediaPollFuture = {};
    MediaState m_mediaState = {};
    bool m_builtinsInstalled = false;

    JSObjectAdapters m_adapters;
};
} // namespace WallpaperEngine::Scripting
