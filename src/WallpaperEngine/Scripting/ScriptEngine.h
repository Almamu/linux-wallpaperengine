#pragma once

#include "Adapters/VectorAdapter.h"
#include "ConsoleObject.h"
#include "EngineObject.h"
#include "InputObject.h"
#include "Modules/ScriptModule.h"
#include "SceneObject.h"

#include <chrono>
#include <functional>
#include <future>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "WallpaperEngine/Data/Model/PropertyAnimation.h"
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
    class VideoTextureAdapter;
}
using namespace WallpaperEngine::Data::Model;

// Opaque handle returned by createLayerScript. 0 means invalid / not created.
using ScriptLayerHandle = int;
static constexpr ScriptLayerHandle kInvalidLayerHandle = 0;

class ScriptEngine {
public:
    struct LoadedModule {
	DynamicValue& value;
	JSValue module;
	ScriptableObject* object = nullptr;
	bool initialized = false;
	glm::vec2 lastOutputSize = { 0, 0 };
	/** Cleared after an authored update hook throws, preventing per-frame retries/log spam. */
	bool updateEnabled = true;
	bool animationEvents = false;
	std::string animationScope;
	bool cursorEvents = false;
	bool cursorInside = false;
	bool cursorPressedInside = false;
	std::optional<Media::MediaSource::MediaInfo> lastMedia;
	std::map<std::string, bool> registeredAssets;
    };
    struct JSObjectAdapters {
	std::unique_ptr<Adapters::VectorAdapter<4>> vec4;
	std::unique_ptr<Adapters::VectorAdapter<3>> vec3;
	std::unique_ptr<Adapters::VectorAdapter<2>> vec2;
	std::unique_ptr<Adapters::ScriptableObjectAdapter> object;
	std::unique_ptr<Adapters::VideoTextureAdapter> video;
    };

    ~ScriptEngine ();
    ScriptEngine (Render::Wallpapers::CScene& scene, Media::MediaSource& mediaSource, const std::string& screenName = "default");
    ScriptEngine (const ScriptEngine&) = delete;
    ScriptEngine& operator= (const ScriptEngine&) = delete;

    JSRuntime* getRuntime () const { return m_runtime; }
    JSContext* getContext () const { return m_context; }
    JSValue getGlobalThis () const { return m_globalThis; }
    bool isInUpdate () const { return m_inUpdate; }
    LoadedModule* getRunningModule () const { return m_runningModule; }
    std::map<std::string, bool>* getRegisteringAssets () const { return m_registeringAssets; }
    // Workshop id exported by the currently-running script module (`export let __workshopId = '...'`),
    // or "" if none. Scripts use it to resolve their own workshop-scoped asset paths at runtime.
    [[nodiscard]] std::string getRunningModuleWorkshopId () const;
    // Fire the scripts' applyUserProperties({key: value}) handler after a live property change, so
    // script-driven wallpapers react to settings without a reload. No-op for scripts without it.
    void dispatchUserProperty (const std::string& key, DynamicValue& value);
    // Hand every script the project's current user properties in one object. Wallpaper Engine does
    // this once at startup, and scripts routinely gate their whole behaviour on it, so skipping it
    // leaves them running their inert default branch forever.
    void dispatchAllUserProperties (LoadedModule* target = nullptr);
    /** Send the initial or changed application language, once per scene-wide broadcast. */
    void dispatchGeneralSettings ();
    void applyUserProperties (JSValue changed, LoadedModule* target = nullptr);
    JSValue dynamicToJs (DynamicValue& value, bool snapshotVectors = false) const;

    /**
     * Evaluate a WallpaperEngine script's update() function.
     *
     * @param key The full JS script text (ES6 module with export function update(value))
     * @param currentValue The current value to pass to update()
     * @return The modified value from update(), or a copy of currentValue on error
     */
    void queueScript (const std::string& key, DynamicValue& currentValue, ScriptableObject& object,
        bool deferInitialization = false);

    /** Register a scene setting and queue its script with a scene property owner. */
    void queueSceneScript (const std::string& property, const UserSetting& setting);

    /** Run init/first-update after the scene has registered every authored layer. */
    void initializeQueuedScripts (ScriptableObject* target = nullptr);
    void finishLoading (std::chrono::steady_clock::duration timerOffset);

    /**
     * Drop every queued script bound to a scriptable object that is going away.
     *
     * queueScript stores a raw ScriptableObject* and a DynamicValue& taken from the object, and
     * hands QuickJS a wrapper holding the same reference. Objects register in their constructor
     * but can still be destroyed afterwards — CScene::dispatchObjectType deletes anything whose
     * setup() throws — so without this the engine keeps running scripts against freed memory.
     */
    void unregisterScriptable (const ScriptableObject* object);
    /** Retire one material property between hooks without invalidating its layer. */
    void unregisterPropertyScript (const std::string& key, const ScriptableObject* object);

    /**
     * Runs a frame tick in the javascript engine. Dispatches any pending events,
     * timeouts, intervals AND calls any update() functions.
     */
    void tick ();
    void dispatchAnimationEvent (ScriptableObject& object, const PropertyAnimation::Event& event,
	const std::string& animationName, const std::string& animationScope = {});

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
    void queueScript (const std::string& key, DynamicValue& currentValue, ScriptableObject* object,
        bool deferInitialization = false, const std::string& sceneProperty = {});
    void callLifecycleHook (const std::string& key, LoadedModule& module, const char* hook);
    void initializeModule (const std::string& key, LoadedModule& module);
    void dispatchScreenResize ();
    void dispatchCursorEvents ();
    JSValue makeCursorEvent (
	const glm::vec3& worldPosition, const glm::vec3& localPosition, const std::optional<std::string>& hitBox
    );

    void installBuiltins ();

    void notifyMediaUpdate (const Media::MediaSource::MediaInfo& media, LoadedModule* target = nullptr);

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
    // Map entries remain stable; the list preserves authored registration order
    // even when a callback creates another layer and appends its scripts.
    std::list<std::pair<const std::string*, LoadedModule*>> m_scriptModuleOrder;

    bool m_inUpdate = false;
    LoadedModule* m_runningModule = nullptr;
    // Non-null only while an authored module's global body is evaluating.
    std::map<std::string, bool>* m_registeringAssets = nullptr;
    bool m_sceneLayersReady = false;
    bool m_initializingScripts = false;
    std::string m_lastGeneralLanguage;
    bool m_cursorLeftDown = false;
    std::optional<glm::vec3> m_lastCursorWorldPosition = std::nullopt;

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
