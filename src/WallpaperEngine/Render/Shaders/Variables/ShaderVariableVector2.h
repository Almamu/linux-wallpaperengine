#pragma once
#include <glm/vec2.hpp>

#include "WallpaperEngine/Render/Shaders/Variables/ShaderVariable.h"

namespace WallpaperEngine::Render::Shaders::Variables {
class ShaderVariableVector2 final : public ShaderVariable {
  public:
    using ShaderVariable::ShaderVariable;

    ShaderVariableVector2 (const glm::vec2& defaultValue, const std::string& name);
};
} // namespace WallpaperEngine::Render::Shaders::Variables
