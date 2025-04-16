#pragma once

#include "CShaderConstant.h"

#include <string>

namespace WallpaperEngine::Core::Objects::Effects::Constants {
/**
 * Shader constant of type float
 */
class CShaderConstantFloat : public CShaderConstant {
  public:
    explicit CShaderConstantFloat (float value);

    /**
     * @return A pointer to the actual value of the constant
     */
    [[nodiscard]] const float* getValue () const;

    /**
     * Type string indicator
     */
    static const std::string Type;

    [[nodiscard]] std::string toString () const override;
  protected:
    /** The constant's value */
    const float m_value;
};
} // namespace WallpaperEngine::Core::Objects::Effects::Constants
