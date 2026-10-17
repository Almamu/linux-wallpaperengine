#pragma once

#include "ObjectAdapter.h"

namespace WallpaperEngine::Scripting::Adapters {
class ScriptableObjectAdapter : public ObjectAdapter {
public:
    explicit ScriptableObjectAdapter (ScriptEngine& engine, std::string name);

    JSValue instantiate (ScriptableObject& object) override;
    JSValue instantiate (Data::Model::DynamicValue& value) override;
    JSValue instantiateAnimationLayer (Data::Model::DynamicValue& rate, Data::Model::DynamicValue& visible);

private:
    JSClassExoticMethods m_exoticMethods;
    JSClassID m_animationLayerClassId;
    std::string m_name;
};
}
