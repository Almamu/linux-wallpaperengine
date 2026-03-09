#pragma once

#include <map>
#include <string>

#include "DynamicValue.h"
#include "Types.h"

namespace WallpaperEngine::Data::Model {

/**
 * A DynamicValue whose value is computed by evaluating a WallpaperEngine script.
 *
 * Holds the script source, a map of script property names to their DynamicValue
 * pointers (connected to user Properties), and the base value from the JSON.
 * When any connected scriptProperty changes, re-evaluates the script.
 */
class ScriptedDynamicValue : public DynamicValue {
public:
	ScriptedDynamicValue (
		std::string scriptSource, std::map<std::string, DynamicValueUniquePtr> scriptProps, DynamicValue baseValue
	);

	~ScriptedDynamicValue () override = default;

private:
	void reevaluate ();

	std::string m_scriptSource;
	std::map<std::string, DynamicValueUniquePtr> m_scriptProps;
	DynamicValue m_baseValue;
};
} // namespace WallpaperEngine::Data::Model
