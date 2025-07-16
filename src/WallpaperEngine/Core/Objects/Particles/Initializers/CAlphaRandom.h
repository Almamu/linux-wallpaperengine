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

    static const CAlphaRandom* fromJSON (const json& data, uint32_t id);

    CAlphaRandom (uint32_t id, double min, double max);

  private:
    /** Maximum alpha */
    const double m_max;
    /** Minimum alpha */
    const double m_min;
};
} // namespace WallpaperEngine::Core::Objects::Particles::Initializers
