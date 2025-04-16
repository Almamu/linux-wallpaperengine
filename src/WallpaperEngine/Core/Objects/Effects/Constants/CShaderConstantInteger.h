#pragma once

#include "CShaderConstant.h"

#include <string>

namespace WallpaperEngine::Core::Objects::Effects::Constants {
/**
 * Shader constant of type integer
 */
class CShaderConstantInteger : public CShaderConstant {
  public:
    explicit CShaderConstantInteger (int32_t value);

    /**
     * @return A pointer to the actual value of the constant
     */
    [[nodiscard]] const int32_t* getValue () const;

    /**
     * Type string indicator
     */
    static const std::string Type;

    [[nodiscard]] std::string toString () const override;
  protected:
    /** The constant's value */
    const int32_t m_value;
};
} // namespace WallpaperEngine::Core::Objects::Effects::Constants
