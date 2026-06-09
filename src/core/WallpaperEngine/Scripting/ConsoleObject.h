#pragma once
#include "quickjs.h"

namespace WallpaperEngine::Render::Wallpapers {
class CScene;
}
namespace WallpaperEngine::Scripting {
class ScriptEngine;
class ConsoleObject {
public:
    ConsoleObject (ScriptEngine& engine, Render::Wallpapers::CScene& scene);
    ~ConsoleObject ();

    const Render::Wallpapers::CScene& getScene () const { return m_scene; }
    JSValue getInstance () const { return m_instance; }

protected:
    Render::Wallpapers::CScene& m_scene;
    ScriptEngine& m_engine;

    JSClassID m_classId;
    JSClassDef m_definition;
    JSValue m_instance;
};
}