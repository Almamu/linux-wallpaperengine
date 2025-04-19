#pragma once
#include <glm/vec2.hpp>

#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariable.h"

namespace WallpaperEngine::Render::Shaders::Variables {
class CShaderVariableVector2 final : public CShaderVariable {
  public:
    explicit CShaderVariableVector2 (const glm::vec2& defaultValue);
    CShaderVariableVector2 (const glm::vec2& defaultValue, const std::string& name);

    static const std::string Type;
};
} // namespace WallpaperEngine::Render::Shaders::Variables
