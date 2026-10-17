#include "ScriptableObject.h"

#include "ScriptEngine.h"
#include "WallpaperEngine/Data/Model/Material.h"
#include "WallpaperEngine/Data/Utils/ScopeGuard.h"

#include <ranges>

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Scripting;

ScriptableObject::ScriptableObject (Wallpapers::CScene& scene, const Object& object) : CObject (scene, object) {
    // Origin is shared by every typed scene object. Scale, angles and visibility
    // are registered by the concrete type because their renderable values live
    // on the typed object, not on ObjectData's generic group fields.
    this->registerProperty ("origin", *object.origin->value);
}

DynamicValue& ScriptableObject::getProperty (const std::string& name) {
    const auto it = this->m_properties.find (name);

    if (it == this->m_properties.end ()) {
	sLog.exception ("Property '" + name + "' not found on object '" + this->getObject ().name + "'");
    }

    return it->second.value;
}

std::optional<ScriptableObject::AnimationLayerProperties>
ScriptableObject::findAnimationLayer (const std::string&) const {
    return std::nullopt;
}

const std::map<std::string, ScriptableObject::PropertyEntry>& ScriptableObject::getProperties () const {
    return this->m_properties;
}

void ScriptableObject::registerProperty (const std::string& name, DynamicValue& value) {
    auto inserted = this->m_properties.emplace (
	name, PropertyEntry { .key = name + "_" + std::to_string (this->getId ()), .value = value }
    );

    if (!inserted.second) {
	return;
    }

    this->getScene ().getScriptEngine ().queueScript (inserted.first->second.key, inserted.first->second.value, *this);
}

void ScriptableObject::registerMaterialProperties (
    const std::string& prefix, const Data::Model::Material& material
) {
    for (size_t passIndex = 0; passIndex < material.passes.size (); passIndex++) {
	const auto& pass = *material.passes[passIndex];
	for (const auto& [name, setting] : pass.constants) {
	    if (setting != nullptr && setting->value != nullptr) {
		this->registerProperty (
		    prefix + "_pass" + std::to_string (passIndex) + "_" + name, *setting->value
		);
	    }
	}
    }
}
