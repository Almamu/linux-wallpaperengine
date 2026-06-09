#include "ScriptModule.h"

#include "WallpaperEngine/Scripting/ScriptEngine.h"

using namespace WallpaperEngine::Scripting::Modules;

ScriptModule::ScriptModule (ScriptEngine& engine, const std::string& name, JSModuleInitFunc* init) :
    m_name (name), m_engine (engine) {
    this->m_definition = JS_NewCModule (this->m_engine.getContext (), this->m_name.c_str (), init);
}

ScriptModule::~ScriptModule () { }