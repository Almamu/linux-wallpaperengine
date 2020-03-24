#include "CAngularVelocityRandom.h"

#include "WallpaperEngine/Core/Core.h"

using namespace WallpaperEngine::Core::Objects::Particles::Initializers;

CAngularVelocityRandom* CAngularVelocityRandom::fromJSON (json data, irr::u32 id)
{
    auto min_it = jsonFindValueRequired(&data, "min", "Angularvelocityrandom initializer must have a minimum value");
    auto max_it = jsonFindValueRequired(&data, "max", "Angularvelocityrandom initializer must have a maximum value");

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

const irr::core::vector3df& CAngularVelocityRandom::getMinimum () const
{
    return this->m_min;
}

const irr::core::vector3df& CAngularVelocityRandom::getMaximum () const
{
    return this->m_max;
}