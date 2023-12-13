#pragma once

#include "WallpaperEngine/Core/Core.h"
#include "WallpaperEngine/Core/Objects/Particles/CInitializer.h"

namespace WallpaperEngine::Core::Objects::Particles::Initializers {
/**
 * Initializer for particles that decides the base alpha for the particles
 */
class CAlphaRandom : CInitializer {
  public:
    /**
     * @return The minimum alpha value to be used
     */
    [[nodiscard]] double getMinimum () const;
    /**
     * @return The maximum alpha value to be used
     */
    [[nodiscard]] double getMaximum () const;

  protected:
    friend class CInitializer;

    static CAlphaRandom* fromJSON (json data, uint32_t id);

    CAlphaRandom (uint32_t id, double min, double max);

  private:
    /** Maximum alpha */
    double m_max;
    /** Minimum alpha */
    double m_min;
};
} // namespace WallpaperEngine::Core::Objects::Particles::Initializers
