#pragma once

#include "ObjectAdapter.h"

namespace WallpaperEngine::Scripting::Adapters {
class ScriptableObjectAdapter : public ObjectAdapter {
public:
    explicit ScriptableObjectAdapter (ScriptEngine& engine, const std::string& name);

    JSValue instantiate (ScriptableObject& object) override;
    JSValue instantiate (Data::Model::DynamicValue& value) override;

private:
    JSClassExoticMethods m_exoticMethods;
    std::string m_name;
};
}