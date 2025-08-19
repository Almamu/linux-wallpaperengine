#pragma once
#include <glm/vec2.hpp>

#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariable.h"

namespace WallpaperEngine::Render::Shaders::Variables {
class CShaderVariableVector2 final : public CShaderVariable {
  public:
    using CShaderVariable::CShaderVariable;

    CShaderVariableVector2 (const glm::vec2& defaultValue, const std::string& name);
};
} // namespace WallpaperEngine::Render::Shaders::Variables
