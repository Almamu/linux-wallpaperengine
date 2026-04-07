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
		const std::string& scriptSource, const std::map<std::string, DynamicValue*>& scriptProperties,
		const DynamicValue& currentValue
	);

private:
	ScriptEngine ();

	JSValue dynamicValueToJS (const DynamicValue& value) const;
	DynamicValueUniquePtr jsToDynamicValue (JSValue val, DynamicValue::UnderlyingType hint) const;

	JSRuntime* m_runtime = nullptr;
	JSContext* m_context = nullptr;
};
} // namespace WallpaperEngine::Scripting
