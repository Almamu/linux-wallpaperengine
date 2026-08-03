#pragma once

#include <glm/vec3.hpp>

#include "WallpaperEngine/Render/Shaders/Variables/ShaderVariable.h"

namespace WallpaperEngine::Render::Shaders::Variables {
class ShaderVariableVector3 final : public ShaderVariable {
public:
    using ShaderVariable::ShaderVariable;

    ShaderVariableVector3 (const glm::vec3& defaultValue, const std::string& name);
};
} // namespace WallpaperEngine::Render::Shaders::Variables
