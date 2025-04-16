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
    [[nodiscard]] const glm::vec4* getValue () const;

    /**
     * Type string indicator
     */
    static const std::string Type;

    [[nodiscard]] std::string toString () const override;
  protected:
    /** The constant's value */
    const glm::vec4 m_value;
};
} // namespace WallpaperEngine::Core::Objects::Effects::Constants
