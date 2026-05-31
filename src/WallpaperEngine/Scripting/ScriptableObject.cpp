#include "ScriptableObject.h"

#include "ScriptEngine.h"
#include "WallpaperEngine/Data/Utils/ScopeGuard.h"

#include <ranges>

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Scripting;

ScriptableObject::ScriptableObject (Wallpapers::CScene& scene, const Object& object) : CObject (scene, object) {
    this->m_context = { .object = { .id = object.id, .name = object.name } };

    // register common dynamic values
    this->registerProperty ("origin", *object.origin->value);
    this->registerProperty ("scale", *object.groupScale->value);
    this->registerProperty ("angles", *object.groupAngles->value);
    this->registerProperty ("visible", *object.groupVisible->value);
}

DynamicValue& ScriptableObject::getProperty (const std::string& name) {
    const auto it = this->m_properties.find (name);

    if (it == this->m_properties.end ()) {
	sLog.exception ("Property '" + name + "' not found on object '" + this->m_context.object.name + "'");
    }

    return it->second;
}

const std::map<std::string, DynamicValue&>& ScriptableObject::getProperties () const { return this->m_properties; }

void ScriptableObject::registerProperty (const std::string& name, DynamicValue& value) {
    this->m_properties.emplace (name, value);
}

void ScriptableObject::reevaluate () {
    if (this->m_evaluating) {
	return;
    }

    this->m_evaluating = true;

    ScopeGuard guard ([this] { this->m_evaluating = false; });

    // evaluate all the properties that need it
    for (auto& property : this->m_properties | std::views::values) {
	const auto& source = property.getScriptSource ();

	if (!source.has_value ()) {
	    continue;
	}

	ScriptEngine::instance ().evaluate (this, *source, property, &this->getScene (), &this->m_context);
    }
}
