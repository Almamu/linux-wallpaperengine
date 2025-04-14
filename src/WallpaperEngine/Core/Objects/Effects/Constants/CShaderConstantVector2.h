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

    /**
     * @return A pointer to the actual value of the constant
     */
    [[nodiscard]] const glm::vec2* getValue () const;

    /**
     * Type string indicator
     */
    static const std::string Type;

  protected:
    /** The constant's value */
    const glm::vec2 m_value;
};
} // namespace WallpaperEngine::Core::Objects::Effects::Constants
