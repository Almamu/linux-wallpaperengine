#pragma once
#include "ScriptModule.h"

namespace WallpaperEngine::Scripting {
class ScriptEngine;
}
namespace WallpaperEngine::Scripting::Modules {
class ColorModule : public ScriptModule {
public:
    ColorModule (ScriptEngine& engine);
    ~ColorModule () override;

protected:
    uint32_t m_instanceId;
};
}