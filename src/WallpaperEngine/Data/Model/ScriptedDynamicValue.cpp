#include "ScriptedDynamicValue.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Scripting/ScriptEngine.h"

using namespace WallpaperEngine::Data::Model;

ScriptedDynamicValue::ScriptedDynamicValue (
    std::string scriptSource,
    std::map<std::string, DynamicValueUniquePtr> scriptProps,
    DynamicValue baseValue
) :
    DynamicValue (),
    m_scriptSource (std::move (scriptSource)),
    m_scriptProps (std::move (scriptProps)),
    m_baseValue (std::move (baseValue)) {
    // Listen for changes on each script property
    for (auto& [name, prop] : this->m_scriptProps) {
	if (prop) {
	    prop->listen ([this] (const DynamicValue&) {
		this->reevaluate ();
	    });
	}
    }

    // Do an initial evaluation
    this->reevaluate ();
}

void ScriptedDynamicValue::reevaluate () {
    // Build raw pointer map for the engine
    std::map<std::string, DynamicValue*> propsMap;
    for (const auto& [name, prop] : this->m_scriptProps) {
	propsMap[name] = prop.get ();
    }

    auto result = WallpaperEngine::Scripting::ScriptEngine::instance ().evaluate (
	this->m_scriptSource, propsMap, this->m_baseValue);

    if (result) {
	this->update (*result);
    }
}
