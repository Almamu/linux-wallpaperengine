#pragma once

#include "CShaderVariable.h"

namespace WallpaperEngine::Render::Shaders::Variables {
class CShaderVariableInteger final : public CShaderVariable {
  public:
    using CShaderVariable::CShaderVariable;

    CShaderVariableInteger (int defaultValue, const std::string& name);
};
} // namespace WallpaperEngine::Render::Shaders::Variables
