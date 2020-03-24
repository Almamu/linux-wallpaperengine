#include "CVelocityRandom.h"

#include "WallpaperEngine/Core/Core.h"

using namespace WallpaperEngine::Core::Objects::Particles::Initializers;

CVelocityRandom* CVelocityRandom::fromJSON (json data, irr::u32 id)
{
    auto min_it = jsonFindValueRequired(&data, "min", "Velocityrandom initializer must have a minimum value");
    auto max_it = jsonFindValueRequired(&data, "max", "Velocityrandom initializer must have a maximum value");

    return new CVelocityRandom (
            id,
            WallpaperEngine::Core::ato3vf (*min_it),
            WallpaperEngine::Core::ato3vf (*max_it)
    );
}


CVelocityRandom::CVelocityRandom (irr::u32 id, irr::core::vector3df min, irr::core::vector3df max) :
        CInitializer (id, "velocityrandom"),
        m_min (min),
        m_max (max)
{
}

const irr::core::vector3df& CVelocityRandom::getMinimum () const
{
    return this->m_min;
}

const irr::core::vector3df& CVelocityRandom::getMaximum () const
{
    return this->m_max;
}