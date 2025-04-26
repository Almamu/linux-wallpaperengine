#pragma once

#include <glm/vec3.hpp>

#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariable.h"

namespace WallpaperEngine::Render::Shaders::Variables {
class CShaderVariableVector3 final : public CShaderVariable {
  public:
    explicit CShaderVariableVector3 (const glm::vec3& defaultValue);
    CShaderVariableVector3 (const glm::vec3& defaultValue, const std::string& name);
};
} // namespace WallpaperEngine::Render::Shaders::Variables
