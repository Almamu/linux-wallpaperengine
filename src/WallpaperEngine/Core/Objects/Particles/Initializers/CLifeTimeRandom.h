#pragma once

#include "WallpaperEngine/Core/Objects/Particles/CInitializer.h"

#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Objects::Particles::Initializers {
/**
 * Initializer for particles that decides the lifetime of each particle on startup
 */
class CLifeTimeRandom : CInitializer {
  public:
    /**
     * @return The minimum lifetime to be used
     */
    [[nodiscard]] uint32_t getMinimum () const;
    /**
     * @return The maximum lifetime to be used
     */
    [[nodiscard]] uint32_t getMaximum () const;

  protected:
    friend class CInitializer;

    static CLifeTimeRandom* fromJSON (json data, uint32_t id);

    CLifeTimeRandom (uint32_t id, uint32_t min, uint32_t max);

  private:
    /** Maximum lifetime */
    uint32_t m_max;
    /** Minimum lifetime */
    uint32_t m_min;
};
} // namespace WallpaperEngine::Core::Objects::Particles::Initializers
