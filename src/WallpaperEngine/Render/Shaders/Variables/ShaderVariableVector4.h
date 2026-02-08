#pragma once
#include <glm/vec4.hpp>

#include "WallpaperEngine/Render/Shaders/Variables/ShaderVariable.h"

namespace WallpaperEngine::Render::Shaders::Variables {
class ShaderVariableVector4 final : public ShaderVariable {
public:
    using ShaderVariable::ShaderVariable;

    ShaderVariableVector4 (const glm::vec4& defaultValue, const std::string& name);
};
} // namespace WallpaperEngine::Render::Shaders::Variables
