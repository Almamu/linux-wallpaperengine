#pragma once
#include "ScriptModule.h"

namespace WallpaperEngine::Scripting {
class ScriptEngine;
}
namespace WallpaperEngine::Scripting::Modules {
class MathModule : public ScriptModule {
public:
    MathModule (ScriptEngine& engine);
    ~MathModule () override;

protected:
    uint32_t m_instanceId;
};
}