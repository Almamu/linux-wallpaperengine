#pragma once
#include <glm/vec4.hpp>

#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariable.h"

namespace WallpaperEngine::Render::Shaders::Variables {
class CShaderVariableVector4 final : public CShaderVariable {
  public:
    explicit CShaderVariableVector4 (const glm::vec4& defaultValue);
    CShaderVariableVector4 (const glm::vec4& defaultValue, const std::string& name);

    static const std::string Type;
};
} // namespace WallpaperEngine::Render::Shaders::Variables
