#pragma once

#include "WallpaperEngine/Core/Objects/Particles/CInitializer.h"

#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Objects::Particles::Initializers {
/**
 * Initializer for particles that decides the base size for the particles
 */
class CSizeRandom : CInitializer {
  public:
    /**
     * @return The minimum size to be used
     */
    [[nodiscard]] uint32_t getMinimum () const;
    /**
     * @return The maximum size to be used
     */
    [[nodiscard]] uint32_t getMaximum () const;

  protected:
    friend class CInitializer;

    static CSizeRandom* fromJSON (json data, uint32_t id);

    CSizeRandom (uint32_t id, uint32_t min, uint32_t max);

  private:
    /** Maximum size */
    uint32_t m_max;
    /** Minimum size */
    uint32_t m_min;
};
} // namespace WallpaperEngine::Core::Objects::Particles::Initializers
