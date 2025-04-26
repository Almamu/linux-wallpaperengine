#pragma once

#include "CShaderVariable.h"

namespace WallpaperEngine::Render::Shaders::Variables {
class CShaderVariableInteger final : public CShaderVariable {
  public:
    explicit CShaderVariableInteger (int32_t defaultValue);
    CShaderVariableInteger (int32_t defaultValue, const std::string& name);
};
} // namespace WallpaperEngine::Render::Shaders::Variables
