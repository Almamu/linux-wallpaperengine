#include "CAlphaRandom.h"

#include "WallpaperEngine/Core/Core.h"

using namespace WallpaperEngine::Core::Objects::Particles::Initializers;

CAlphaRandom* CAlphaRandom::fromJSON (json data, irr::u32 id)
{
    auto min_it = jsonFindRequired (&data, "min", "Alpharandom initializer must have a minimum value");
    auto max_it = jsonFindRequired (&data, "max", "Alpharandom initializer must have a maximum value");

    return new CAlphaRandom (id, *min_it, *max_it);
}

CAlphaRandom::CAlphaRandom (irr::u32 id, irr::f64 min, irr::f64 max) :
        CInitializer (id, "alpharandom"),
        m_min (min),
        m_max (max)
{
}

const irr::f64 CAlphaRandom::getMinimum () const
{
    return this->m_min;
}

const irr::f64 CAlphaRandom::getMaximum () const
{
    return this->m_max;
}