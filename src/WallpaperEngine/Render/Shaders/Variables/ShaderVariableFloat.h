#pragma once

#include "ShaderVariable.h"

namespace WallpaperEngine::Render::Shaders::Variables {
class ShaderVariableFloat final : public ShaderVariable {
  public:
    using ShaderVariable::ShaderVariable;

    ShaderVariableFloat (float defaultValue, const std::string& name);
};
} // namespace WallpaperEngine::Render::Shaders::Variables
