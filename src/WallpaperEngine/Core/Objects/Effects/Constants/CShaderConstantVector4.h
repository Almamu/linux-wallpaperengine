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

    [[nodiscard]] std::string toString () const override;
};
} // namespace WallpaperEngine::Core::Objects::Effects::Constants
