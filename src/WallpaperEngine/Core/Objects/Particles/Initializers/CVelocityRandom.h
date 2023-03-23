#pragma once

#include "WallpaperEngine/Core/Objects/Particles/CInitializer.h"

#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Objects::Particles::Initializers
{
    /**
     * Initializer for particles that decides the base velocity for the particles
     */
    class CVelocityRandom : CInitializer
    {
    public:
        /**
         * @return The minimum velocity (direction * speed)
         */
        [[nodiscard]] const glm::vec3& getMinimum () const;
        /**
         * @return The maximum velocity (direction * speed)
         */
        [[nodiscard]] const glm::vec3& getMaximum () const;

    protected:
        friend class CInitializer;

        static CVelocityRandom* fromJSON (json data, uint32_t id);

        CVelocityRandom (uint32_t id, glm::vec3 min, glm::vec3 max);

    private:
        /** Maximum velocity */
        glm::vec3 m_max;
        /** Minimum velocity */
        glm::vec3 m_min;
    };
}
