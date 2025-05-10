#pragma once

#include "CShaderVariable.h"

namespace WallpaperEngine::Render::Shaders::Variables {
class CShaderVariableFloat final : public CShaderVariable {
  public:
    explicit CShaderVariableFloat (float defaultValue);
    CShaderVariableFloat (float defaultValue, const std::string& name);
};
} // namespace WallpaperEngine::Render::Shaders::Variables
