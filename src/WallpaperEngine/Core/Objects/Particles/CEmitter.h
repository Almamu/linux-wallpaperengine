#pragma once

#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Objects::Particles {
using json = nlohmann::json;

/**
 * Particle emitter, controls the area where the particles have to be created
 */
class CEmitter {
  public:
    static CEmitter* fromJSON (json data);

    /**
     * @return The ID of the emitter
     */
    [[nodiscard]] uint32_t getId () const;
    /**
     * @return The name of the emitter, indicates what type of emission to do
     */
    [[nodiscard]] const std::string& getName () const;
    /**
     * @return The maximum distance a particle can travel before being dead
     */
    [[nodiscard]] const uint32_t getDistanceMax () const;
    /**
     * @return The minimum distance a particle can travel before being dead
     */
    [[nodiscard]] const uint32_t getDistanceMin () const;
    /**
     * @return The direction a particle should move to
     */
    [[nodiscard]] const glm::vec3& getDirections () const;
    /**
     * @return The center of the particle emission
     */
    [[nodiscard]] const glm::vec3& getOrigin () const;
    /**
     * @return The rate of particle emission
     */
    [[nodiscard]] const double getRate () const;

  protected:
    CEmitter (const glm::vec3& directions, uint32_t distancemax, uint32_t distancemin, uint32_t id, std::string name,
              const glm::vec3& origin, double rate);

  private:
    /** Direction the particles should move to */
    glm::vec3 m_directions;
    /** Maximum distance before the particle is dead */
    uint32_t m_distancemax;
    /** Minimum distance before the particle is dead */
    uint32_t m_distancemin;
    /** ID of the emitter */
    uint32_t m_id;
    /** Name of the emitter, indicates the type of emitter */
    std::string m_name;
    /** The center of the emitter */
    glm::vec3 m_origin;
    /** The rate of emission */
    double m_rate;
};
} // namespace WallpaperEngine::Core::Objects::Particles
