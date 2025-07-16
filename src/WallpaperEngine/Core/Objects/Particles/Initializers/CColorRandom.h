#pragma once

#include "WallpaperEngine/Core/Objects/Particles/CInitializer.h"

#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Objects::Particles::Initializers {
/**
 * Initializer for particles that decides the base color
 */
class CColorRandom : CInitializer {
  public:
    /**
     * @return The minimum color to use (RGB)
     */
    [[nodiscard]] const glm::ivec3& getMinimum () const;
    /**
     * @return The maximum color to use (RGB)
     */
    [[nodiscard]] const glm::ivec3& getMaximum () const;

  protected:
    friend class CInitializer;

    static const CColorRandom* fromJSON (const json& data, uint32_t id);

    CColorRandom (uint32_t id, glm::ivec3 min, glm::ivec3 max);

  private:
    /** Maximum color */
    const glm::ivec3 m_max;
    /** Minimum color */
    const glm::ivec3 m_min;
};
} // namespace WallpaperEngine::Core::Objects::Particles::Initializers
