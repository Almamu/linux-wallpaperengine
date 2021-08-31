#include "CLifeTimeRandom.h"

using namespace WallpaperEngine::Core::Objects::Particles::Initializers;

CLifeTimeRandom* CLifeTimeRandom::fromJSON (json data, uint32_t id)
{
    auto min_it = jsonFindRequired (data, "min", "Lifetimerandom initializer must have a minimum value");
    auto max_it = jsonFindRequired (data, "max", "Lifetimerandom initializer must have a maximum value");

    return new CLifeTimeRandom (id, *min_it, *max_it);
}


CLifeTimeRandom::CLifeTimeRandom (uint32_t id, uint32_t min, uint32_t max) :
        CInitializer (id, "lifetimerandom"),
        m_min (min),
        m_max (max)
{
}

const uint32_t CLifeTimeRandom::getMinimum () const
{
    return this->m_min;
}

const uint32_t CLifeTimeRandom::getMaximum () const
{
    return this->m_max;
}