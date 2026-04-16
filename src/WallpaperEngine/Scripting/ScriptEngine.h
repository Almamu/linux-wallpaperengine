#pragma once

#include <map>
#include <memory>
#include <string>

#include "WallpaperEngine/Data/Model/DynamicValue.h"
#include "WallpaperEngine/Data/Model/Types.h"

extern "C" {
#include "quickjs.h"
}

namespace WallpaperEngine::Scripting {
using namespace WallpaperEngine::Data::Model;

// Opaque handle returned by createLayerScript. 0 means invalid / not created.
using ScriptLayerHandle = int;
static constexpr ScriptLayerHandle kInvalidLayerHandle = 0;

class ScriptEngine {
public:
    static ScriptEngine& instance ();

    ~ScriptEngine ();
    ScriptEngine (const ScriptEngine&) = delete;
    ScriptEngine& operator= (const ScriptEngine&) = delete;

    /**
     * Evaluate a WallpaperEngine script's update() function.
     *
     * @param scriptSource The full JS script text (ES6 module with export function update(value))
     * @param scriptProperties Map of property name to current DynamicValue*
     * @param currentValue The current value to pass to update()
     * @return The modified value from update(), or a copy of currentValue on error
     */
    DynamicValueUniquePtr evaluate (
	const std::string& scriptSource,
	const std::map<std::string, DynamicValue*>& scriptProperties,
	const DynamicValue& currentValue
    );

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
	const std::string& scriptSource,
	const std::map<std::string, DynamicValue*>& initialScriptProps,
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

private:
    ScriptEngine ();

    JSValue dynamicValueToJS (const DynamicValue& value) const;
    DynamicValueUniquePtr jsToDynamicValue (JSValue val, DynamicValue::UnderlyingType hint) const;

    // Installs globalThis.__layers and related helpers. Called lazily.
    void ensureLayerRegistry ();

    JSRuntime* m_runtime = nullptr;
    JSContext* m_context = nullptr;
    ScriptLayerHandle m_nextLayerId = 1;
    bool m_layerRegistryReady = false;
    std::map<ScriptLayerHandle, bool> m_layerInitialized;
};
} // namespace WallpaperEngine::Scripting
