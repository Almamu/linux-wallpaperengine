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
    // TODO: SUPPORT DEFAULT VALUE?
    explicit CShaderConstantProperty (const CProperty* property);

    /**
     * @return The property this points to
     */
    [[nodiscard]] const CProperty* getProperty () const;

    /**
     * Type string indicator
     */
    static const std::string Type;

    [[nodiscard]] std::string toString () const override;

  protected:
    /**
     * The backing property
     */
    const CProperty* m_property;
};
} // namespace WallpaperEngine::Core::Objects::Effects::Constants
