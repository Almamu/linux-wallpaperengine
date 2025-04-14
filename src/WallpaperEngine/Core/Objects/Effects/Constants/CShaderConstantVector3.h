#pragma once

#include "CShaderConstant.h"

#include <glm/vec3.hpp>
#include <string>

namespace WallpaperEngine::Core::Objects::Effects::Constants {
/**
 * Shader constant of vector3 type
 */
class CShaderConstantVector3 : public CShaderConstant {
  public:
    explicit CShaderConstantVector3 (glm::vec3 value);

    /**
     * @return A pointer to the actual value of the constant
     */
    [[nodiscard]] const glm::vec3* getValue () const;

    /**
     * Type string indicator
     */
    static const std::string Type;

  protected:
    /** The constant's value */
    const glm::vec3 m_value;
};
} // namespace WallpaperEngine::Core::Objects::Effects::Constants
