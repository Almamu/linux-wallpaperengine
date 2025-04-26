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

    [[nodiscard]] std::string toString () const override;
};
} // namespace WallpaperEngine::Core::Objects::Effects::Constants
