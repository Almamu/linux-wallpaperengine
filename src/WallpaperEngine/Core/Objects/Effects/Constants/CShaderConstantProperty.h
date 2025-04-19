#pragma once

#include "WallpaperEngine/Core/Projects/CProperty.h"

#include "CShaderConstant.h"

#include <string>

namespace WallpaperEngine::Core::Objects::Effects::Constants {
using namespace WallpaperEngine::Core::Projects;
/**
 * Shader constant that comes from a property
 */
class CShaderConstantProperty : public CShaderConstant {
  public:
    explicit CShaderConstantProperty (const CProperty* property);

    /**
     * Type string indicator
     */
    static const std::string Type;

    [[nodiscard]] std::string toString () const override;
};
} // namespace WallpaperEngine::Core::Objects::Effects::Constants
