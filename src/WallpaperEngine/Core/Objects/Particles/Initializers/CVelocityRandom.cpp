#include "CVelocityRandom.h"

#include "WallpaperEngine/Core/Core.h"

using namespace WallpaperEngine::Core::Objects::Particles::Initializers;

CVelocityRandom* CVelocityRandom::fromJSON (json data, irr::u32 id)
{
    json::const_iterator min_it = data.find ("min");
    json::const_iterator max_it = data.find ("max");

    if (min_it == data.end ())
    {
        throw std::runtime_error ("Velocityrandom initializer must have a minimum value");
    }

    if (max_it == data.end ())
    {
        throw std::runtime_error ("Velocityrandom initializer must have a maximum value");
    }

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

irr::core::vector3df* CVelocityRandom::getMinimum ()
{
    return &this->m_min;
}

irr::core::vector3df* CVelocityRandom::getMaximum ()
{
    return &this->m_max;
}