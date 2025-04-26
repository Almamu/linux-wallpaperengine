#pragma once

#include "CShaderConstant.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <string>

namespace WallpaperEngine::Core::Objects::Effects::Constants {
/**
 * Shader constant of vector2 type
 */
class CShaderConstantVector2 : public CShaderConstant {
  public:
    explicit CShaderConstantVector2 (glm::vec2 value);

    [[nodiscard]] std::string toString () const override;
};
} // namespace WallpaperEngine::Core::Objects::Effects::Constants
