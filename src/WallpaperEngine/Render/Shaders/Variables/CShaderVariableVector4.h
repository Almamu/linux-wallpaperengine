#pragma once
#include <glm/vec4.hpp>

#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariable.h"

namespace WallpaperEngine::Render::Shaders::Variables {
class CShaderVariableVector4 final : public CShaderVariable {
  public:
    using CShaderVariable::CShaderVariable;

    CShaderVariableVector4 (const glm::vec4& defaultValue, const std::string& name);
};
} // namespace WallpaperEngine::Render::Shaders::Variables
