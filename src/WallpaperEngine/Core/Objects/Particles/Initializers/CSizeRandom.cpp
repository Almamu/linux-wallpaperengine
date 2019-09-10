#include "CSizeRandom.h"

using namespace WallpaperEngine::Core::Objects::Particles::Initializers;

CSizeRandom* CSizeRandom::fromJSON (json data, irr::u32 id)
{
    auto min_it = data.find ("min");
    auto max_it = data.find ("max");

    if (min_it == data.end ())
    {
        throw std::runtime_error ("Sizerandom initializer must have a minimum value");
    }

    if (max_it == data.end ())
    {
        throw std::runtime_error ("Sizerandom initializer must have a maximum value");
    }

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