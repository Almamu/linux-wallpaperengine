#pragma once

#include "WallpaperEngine/Core/Core.h"
#include "WallpaperEngine/Core/Objects/Particles/CInitializer.h"

namespace WallpaperEngine::Core::Objects::Particles::Initializers {
/**
 * Initializer for particles that decides the base angular velocity for particles
 */
class CAngularVelocityRandom : CInitializer {
  public:
    /**
     * @return Minimum angular velocity (direction * speed)
     */
    [[nodiscard]] const glm::vec3& getMinimum () const;
    /**
     * @return Maximum angular velocity (direction * speed)
     */
    [[nodiscard]] const glm::vec3& getMaximum () const;

  protected:
    friend class CInitializer;

    static CAngularVelocityRandom* fromJSON (json data, uint32_t id);

    CAngularVelocityRandom (uint32_t id, glm::vec3 min, glm::vec3 max);

  private:
    /** Maximum velocity (direction * speed) */
    glm::vec3 m_max;
    /** Minimum velocity (direction * speed) */
    glm::vec3 m_min;
};
} // namespace WallpaperEngine::Core::Objects::Particles::Initializers
