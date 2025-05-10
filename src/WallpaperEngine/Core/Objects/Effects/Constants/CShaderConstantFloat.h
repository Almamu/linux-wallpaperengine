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

    [[nodiscard]] std::string toString () const override;
};
} // namespace WallpaperEngine::Core::Objects::Effects::Constants
