#pragma once

#include "WallpaperEngine/Core/Objects/Particles/CInitializer.h"

#include <nlohmann/json.hpp>

namespace WallpaperEngine::Core::Objects::Particles::Initializers {
/**
 * Initializer for particles that decides the turbulent velocity for the particles
 */
class CTurbulentVelocityRandom : CInitializer {
  public:
    /**
     * @return The phase to use
     */
    double getPhaseMax () const;
    /**
     * @return The scale to use
     */
    double getScale () const;
    /**
     * @return How time affects to the scale
     */
    double getTimeScale () const;
    /**
     * @return The minimum speed
     */
    uint32_t getMinimumSpeed () const;
    /**
     * @return The maximum speed
     */
    uint32_t getMaximumSpeed () const;

  protected:
    friend class CInitializer;

    static const CTurbulentVelocityRandom* fromJSON (const json& data, uint32_t id);

    CTurbulentVelocityRandom (uint32_t id, double phasemax, double scale, double timescale, uint32_t speedmin,
                              uint32_t speedmax);

  private:
    /** Phase */
    const double m_phasemax;
    /** Scale */
    const double m_scale;
    /** Time scale, how the time affects the scale */
    const double m_timescale;
    /** Minimum speed */
    const uint32_t m_speedmin;
    /** Maximum speed */
    const uint32_t m_speedmax;
};
} // namespace WallpaperEngine::Core::Objects::Particles::Initializers
