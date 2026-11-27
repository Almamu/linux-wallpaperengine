#pragma once

#include "ObjectAdapter.h"
#include <map>

namespace WallpaperEngine::Scripting::Adapters {
class ScriptableObjectAdapter : public ObjectAdapter {
public:
    explicit ScriptableObjectAdapter (ScriptEngine& engine, std::string name);
    ~ScriptableObjectAdapter () override;

    JSValue instantiate (ScriptableObject& object) override;
    JSValue instantiate (Data::Model::DynamicValue& value) override;
    JSValue instantiateEffect (JSValueConst owner, int32_t index);
    JSValue instantiateEffectVisibilityOwner (JSValueConst owner, const Data::Model::DynamicValue& value);
    void invalidate (const ScriptableObject* object);

    // Recover the underlying ScriptableObject from a JS layer value produced by instantiate(),
    // or nullptr if the value isn't one of ours. Used by scene-script layer APIs that take a
    // layer handle (getLayerIndex/sortLayer).
    static ScriptableObject* fromJS (JSValue value);

private:
    JSClassExoticMethods m_exoticMethods;
    JSClassID m_effectClassId = 0;
    std::string m_name;
    std::map<const ScriptableObject*, JSValue> m_instances;
};
}
