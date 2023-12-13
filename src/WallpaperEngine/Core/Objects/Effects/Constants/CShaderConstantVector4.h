#pragma once

#include "CShaderConstant.h"

#include <glm/vec4.hpp>
#include <string>

namespace WallpaperEngine::Core::Objects::Effects::Constants {
/**
 * Shader constant of vector4 type
 */
class CShaderConstantVector4 : public CShaderConstant {
  public:
    explicit CShaderConstantVector4 (glm::vec4 value);

    /**
     * @return A pointer to the actual value of the constant
     */
    glm::vec4* getValue ();

    /**
     * Type string indicator
     */
    static const std::string Type;

  protected:
    /** The constant's value */
    glm::vec4 m_value;
};
} // namespace WallpaperEngine::Core::Objects::Effects::Constants
