#include "CSizeRandom.h"

#include "WallpaperEngine/Core/Core.h"

using namespace WallpaperEngine::Core::Objects::Particles::Initializers;

CSizeRandom* CSizeRandom::fromJSON (json data, irr::u32 id)
{
    auto min_it = jsonFindRequired (&data, "min", "Sizerandom initializer must have a minimum value");
    auto max_it = jsonFindRequired (&data, "max", "Sizerandom initializer must have a maximum value");

    return new CSizeRandom (id, *min_it, *max_it);
}

CSizeRandom::CSizeRandom (irr::u32 id, irr::u32 min, irr::u32 max) :
        CInitializer (id, "sizerandom"),
        m_min (min),
        m_max (max)
{
}

const irr::u32 CSizeRandom::getMinimum () const
{
    return this->m_min;
}

const irr::u32 CSizeRandom::getMaximum () const
{
    return this->m_max;
}