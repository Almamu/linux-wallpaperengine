#include "CAngularVelocityRandom.h"

#include "WallpaperEngine/Core/Core.h"

using namespace WallpaperEngine::Core::Objects::Particles::Initializers;

CAngularVelocityRandom* CAngularVelocityRandom::fromJSON (json data, irr::u32 id)
{
    json::const_iterator min_it = data.find ("min");
    json::const_iterator max_it = data.find ("max");

    if (min_it == data.end ())
    {
        throw std::runtime_error ("Angularvelocityrandom initializer must have a minimum value");
    }

    if (max_it == data.end ())
    {
        throw std::runtime_error ("Angularvelocityrandom initializer must have a maximum value");
    }

    return new CAngularVelocityRandom (
            id,
            WallpaperEngine::Core::ato3vf (*min_it),
            WallpaperEngine::Core::ato3vf (*max_it)
    );
}


CAngularVelocityRandom::CAngularVelocityRandom (irr::u32 id, irr::core::vector3df min, irr::core::vector3df max) :
        CInitializer (id, "angularvelocityrandom"),
        m_min (min),
        m_max (max)
{
}

irr::core::vector3df* CAngularVelocityRandom::getMinimum ()
{
    return &this->m_min;
}

irr::core::vector3df* CAngularVelocityRandom::getMaximum ()
{
    return &this->m_max;
}