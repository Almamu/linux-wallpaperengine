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

ScriptedDynamicValue::~ScriptedDynamicValue () {
    WallpaperEngine::Scripting::ScriptEngine::instance ().releaseBinding (this);
}

void ScriptedDynamicValue::setBindingContext (ScriptBindingContext context) {
    this->m_bindingContext = std::move (context);
}

const std::optional<ScriptBindingContext>& ScriptedDynamicValue::getBindingContext () const {
    return this->m_bindingContext;
}

void ScriptedDynamicValue::reevaluate () { this->reevaluate (nullptr); }

void ScriptedDynamicValue::reevaluate (WallpaperEngine::Render::Wallpapers::CScene* scene) {
    if (this->m_evaluating) {
	return;
    }

    this->m_evaluating = true;

    // Build raw pointer map for the engine
    std::map<std::string, DynamicValue*> propsMap;
    for (const auto& [name, prop] : this->m_scriptProps) {
	propsMap[name] = prop.get ();
    }

    const DynamicValue& currentValue = this->getType () == DynamicValue::Null ? this->m_baseValue : *this;

    auto result = WallpaperEngine::Scripting::ScriptEngine::instance ().evaluate (
	this,
	this->m_scriptSource,
	propsMap,
	currentValue,
	scene,
	this->m_bindingContext.has_value () ? &this->m_bindingContext.value () : nullptr
    );

    if (result) {
	this->update (*result);
    }

    this->m_evaluating = false;
}
