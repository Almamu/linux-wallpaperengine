#pragma once

#include "quickjs.h"

#include <string>

namespace WallpaperEngine::Scripting {
class ScriptEngine;
}
namespace WallpaperEngine::Scripting::Modules {
class ScriptModule {
public:
    virtual ~ScriptModule ();

    ScriptEngine& getEngine () const { return m_engine; }
    const std::string& getName () const { return m_name; }
    JSModuleDef* getDefinition () const { return m_definition; }

protected:
    ScriptModule (ScriptEngine& engine, const std::string& name, JSModuleInitFunc* init);

    const std::string m_name;
    ScriptEngine& m_engine;
    JSModuleDef* m_definition;
};
}