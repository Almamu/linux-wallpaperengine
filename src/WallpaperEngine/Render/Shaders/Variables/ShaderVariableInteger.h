#pragma once

#include "ShaderVariable.h"

namespace WallpaperEngine::Render::Shaders::Variables {
class ShaderVariableInteger final : public ShaderVariable {
  public:
    using ShaderVariable::ShaderVariable;

    ShaderVariableInteger (int defaultValue, const std::string& name);
};
} // namespace WallpaperEngine::Render::Shaders::Variables
