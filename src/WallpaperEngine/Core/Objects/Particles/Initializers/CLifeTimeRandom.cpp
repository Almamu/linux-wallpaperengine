#include "CLifeTimeRandom.h"

using namespace WallpaperEngine::Core::Objects::Particles::Initializers;

CLifeTimeRandom* CLifeTimeRandom::fromJSON (json data, irr::u32 id)
{
    auto min_it = jsonFindRequired (data, "min", "Lifetimerandom initializer must have a minimum value");
    auto max_it = jsonFindRequired (data, "max", "Lifetimerandom initializer must have a maximum value");

    return new CLifeTimeRandom (id, *min_it, *max_it);
}


CLifeTimeRandom::CLifeTimeRandom (irr::u32 id, irr::u32 min, irr::u32 max) :
        CInitializer (id, "lifetimerandom"),
        m_min (min),
        m_max (max)
{
}

const irr::u32 CLifeTimeRandom::getMinimum () const
{
    return this->m_min;
}

const irr::u32 CLifeTimeRandom::getMaximum () const
{
    return this->m_max;
}