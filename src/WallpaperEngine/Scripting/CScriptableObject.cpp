#include "CScriptableObject.h"

#include "ScriptEngine.h"
#include "WallpaperEngine/Data/Utils/ScopeGuard.h"

#include <ranges>

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Scripting;

CScriptableObject::CScriptableObject (Wallpapers::CScene& scene, const Object& object)
    : CObject (scene, object) {
    this->m_context = {
        .object = {
            .id = object.id,
            .name = object.name
        }
    };

    // register common dynamic values
    this->registerProperty ("origin", *object.origin->value);
    this->registerProperty ("scale", *object.groupScale->value);
    this->registerProperty ("angles", *object.groupAngles->value);
    this->registerProperty ("visible", *object.groupVisible->value);
}

void CScriptableObject::registerProperty (const std::string& name, DynamicValue& value) {
    this->m_properties.emplace (name, value);
}

void CScriptableObject::reevaluate () {
    if (this->m_evaluating) {
        return;
    }

    this->m_evaluating = true;

    ScopeGuard guard([this] {
        this->m_evaluating = false;
    });

    // evaluate all the properties that need it
    for (auto& property : this->m_properties | std::views::values) {
        const auto& source = property.getScriptSource ();

        if (!source.has_value()) {
            continue;
        }

        ScriptEngine::instance ().evaluate (
	    this,
	    *source,
	    this->m_properties,
	    property,
	    &this->getScene (),
	    &this->m_context
        );
    }
}
