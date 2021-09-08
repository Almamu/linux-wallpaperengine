#include "CVelocityRandom.h"

using namespace WallpaperEngine::Core::Objects::Particles::Initializers;

CVelocityRandom* CVelocityRandom::fromJSON (json data, uint32_t id)
{
    auto min_it = jsonFindRequired (data, "min", "Velocityrandom initializer must have a minimum value");
    auto max_it = jsonFindRequired (data, "max", "Velocityrandom initializer must have a maximum value");

    return new CVelocityRandom (
            id,
            WallpaperEngine::Core::aToVector3 (*min_it),
            WallpaperEngine::Core::aToVector3 (*max_it)
    );
}


CVelocityRandom::CVelocityRandom (uint32_t id, glm::vec3 min, glm::vec3 max) :
        CInitializer (id, "velocityrandom"),
        m_min (min),
        m_max (max)
{
}

const glm::vec3& CVelocityRandom::getMinimum () const
{
    return this->m_min;
}

const glm::vec3& CVelocityRandom::getMaximum () const
{
    return this->m_max;
}