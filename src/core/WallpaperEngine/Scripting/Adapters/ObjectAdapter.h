#pragma once

#include "../../Data/Utils/TypeCaster.h"
#include "quickjs.h"

namespace WallpaperEngine::Data::Model {
class DynamicValue;
}
namespace WallpaperEngine::Scripting {
class ScriptEngine;
class ScriptableObject;
}

namespace WallpaperEngine::Scripting::Adapters {
class ObjectAdapter : public Data::Utils::TypeCaster {
public:
    explicit ObjectAdapter (ScriptEngine& engine);
    ~ObjectAdapter () override = default;

    virtual JSValue instantiate (ScriptableObject& object);
    virtual JSValue instantiate (Data::Model::DynamicValue& value);

    ScriptEngine& getEngine () const { return m_engine; }

protected:
    void registerType (const JSClassDef& definition);

    ScriptEngine& m_engine;
    JSClassID m_classId;
    JSClassDef m_definition;
};
}