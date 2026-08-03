#pragma once
#include "quickjs.h"

namespace WallpaperEngine::Render::Wallpapers {
class CScene;
}
namespace WallpaperEngine::Scripting {
class ScriptEngine;
class ScriptPropertiesObject {
public:
    ScriptPropertiesObject (ScriptEngine& engine, Render::Wallpapers::CScene& scene);
    ~ScriptPropertiesObject ();

    [[nodiscard]] const Render::Wallpapers::CScene& getScene () const { return m_scene; }
    [[nodiscard]] JSValue getCreatorPrototype () const { return m_creatorPrototype; }
    [[nodiscard]] JSValue getPropertiesPrototype () const { return m_propertiesPrototype; }
    [[nodiscard]] JSClassID getCreatorClassId () const { return m_creatorClassId; }
    [[nodiscard]] JSClassID getPropertiesClassId () const { return m_propertiesClassId; }
    [[nodiscard]] ScriptEngine& getEngine () const { return m_engine; }

protected:
    Render::Wallpapers::CScene& m_scene;
    ScriptEngine& m_engine;

    uint32_t m_instanceId;

    JSClassID m_creatorClassId;
    JSClassDef m_creatorDefinition;
    JSValue m_creatorPrototype;

    JSClassID m_propertiesClassId;
    JSClassDef m_propertiesDefinition;
    JSValue m_propertiesPrototype;
    JSClassExoticMethods m_exoticMethods;
};
}