#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "WallpaperEngine/Data/Model/DynamicValue.h"
#include "WallpaperEngine/Data/Model/ScriptedDynamicValue.h"
#include "WallpaperEngine/Data/Model/Types.h"

extern "C" {
#include "quickjs.h"
}

namespace WallpaperEngine::Render::Wallpapers {
class CScene;
}

namespace WallpaperEngine::Scripting {
using namespace WallpaperEngine::Data::Model;

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
	const void* bindingKey,
	const std::string& scriptSource,
	const std::map<std::string, DynamicValue*>& scriptProperties,
	const DynamicValue& currentValue,
	WallpaperEngine::Render::Wallpapers::CScene* scene = nullptr,
	const ScriptBindingContext* bindingContext = nullptr
    );

    JSValue dynamicValueToJS (const DynamicValue& value) const;
    DynamicValueUniquePtr jsToDynamicValue (JSValue val, DynamicValue::UnderlyingType hint) const;
    void releaseBinding (const void* bindingKey);

private:
    ScriptEngine ();

    JSValue ensureModule (const void* bindingKey, const std::string& scriptSource);
    void installBuiltins ();
    void refreshMediaState ();
    void dispatchMediaEvents (JSValue module, const void* bindingKey);
    void updateRuntimeGlobals (JSContext* ctx, JSValue globalObj) const;
    void updateSceneInputGlobals (JSContext* ctx, JSValue globalObj, WallpaperEngine::Render::Wallpapers::CScene* scene);
    void callModuleWithProps (JSContext* ctx, JSValue module, const char* name, JSValue propsObj) const;
    void initializeModuleIfNeeded (JSContext* ctx, JSValue module, const void* bindingKey, const DynamicValue& currentValue, bool hasScene);
    void runIntervals (JSContext* ctx, JSValue globalObj, const std::string& bindingKeyString) const;
    DynamicValueUniquePtr fallbackTextValue (const std::string& scriptSource) const;
    void applyTextFallback (DynamicValue& value, const std::string& scriptSource) const;
    void logTextEvaluationDebug (const ScriptBindingContext* bindingContext) const;
    void logTextResultDebug (const ScriptBindingContext* bindingContext, const DynamicValueUniquePtr& dynResult) const;
    JSValue callUpdate (JSContext* ctx, JSValue module, const DynamicValue& currentValue) const;
    DynamicValueUniquePtr resolveUndefinedResult (
	const std::string& scriptSource,
	const DynamicValue& currentValue,
	WallpaperEngine::Render::Wallpapers::CScene* scene,
	const ScriptBindingContext* bindingContext
    ) const;
    DynamicValueUniquePtr resolveEvaluationResult (
	JSContext* ctx,
	JSValue result,
	const std::string& scriptSource,
	const DynamicValue& currentValue,
	WallpaperEngine::Render::Wallpapers::CScene* scene,
	const ScriptBindingContext* bindingContext
    ) const;

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

    JSRuntime* m_runtime = nullptr;
    JSContext* m_context = nullptr;
    std::unordered_map<const void*, JSValue> m_modules = {};
    std::unordered_set<const void*> m_initializedModules = {};
    std::unordered_map<const void*, std::string> m_lastMediaProperties = {};
    std::unordered_map<const void*, std::string> m_lastMediaPlayback = {};
    std::unordered_map<const void*, std::string> m_lastMediaTimeline = {};
    std::unordered_map<const void*, std::string> m_lastMediaThumbnail = {};
    std::chrono::steady_clock::time_point m_lastMediaPoll = {};
    MediaState m_mediaState = {};
    bool m_builtinsInstalled = false;
};
} // namespace WallpaperEngine::Scripting
