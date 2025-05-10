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

    [[nodiscard]] std::string toString () const override;
};
} // namespace WallpaperEngine::Core::Objects::Effects::Constants
