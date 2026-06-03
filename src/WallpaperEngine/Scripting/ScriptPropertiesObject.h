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

    const Render::Wallpapers::CScene& getScene () const { return m_scene; }
    JSValue getInstance () const { return m_creatorInstance; }

protected:
    Render::Wallpapers::CScene& m_scene;
    ScriptEngine& m_engine;

    JSClassID m_creatorClassId;
    JSClassDef m_creatorDefinition;
    JSValue m_creatorInstance;

    JSClassID m_propertiesClassId;
    JSClassDef m_propertiesDefinition;
    JSValue m_propertiesInstance;
};
}