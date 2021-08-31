#include "CSizeRandom.h"

using namespace WallpaperEngine::Core::Objects::Particles::Initializers;

CSizeRandom* CSizeRandom::fromJSON (json data, uint32_t id)
{
    auto min_it = jsonFindRequired (data, "min", "Sizerandom initializer must have a minimum value");
    auto max_it = jsonFindRequired (data, "max", "Sizerandom initializer must have a maximum value");

    return new CSizeRandom (id, *min_it, *max_it);
}

CSizeRandom::CSizeRandom (uint32_t id, uint32_t min, uint32_t max) :
        CInitializer (id, "sizerandom"),
        m_min (min),
        m_max (max)
{
}

const uint32_t CSizeRandom::getMinimum () const
{
    return this->m_min;
}

const uint32_t CSizeRandom::getMaximum () const
{
    return this->m_max;
}