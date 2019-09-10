#include "CRotationRandom.h"

using namespace WallpaperEngine::Core::Objects::Particles::Initializers;

CRotationRandom* CRotationRandom::fromJSON (json data, irr::u32 id)
{
    auto min_it = data.find ("min");
    auto max_it = data.find ("max");

    irr::f64 min = 0.0f;
    irr::f64 max = 360.0f;

    if (min_it != data.end ())
    {
        min = *min_it;
    }

    if (max_it != data.end ())
    {
        max = *max_it;
    }

    return new CRotationRandom (id, min, max);
}

CRotationRandom::CRotationRandom (irr::u32 id, irr::f64 min, irr::f64 max) :
        CInitializer (id, "rotationrandom"),
        m_min (min),
        m_max (max)
{
}

const irr::f64 CRotationRandom::getMinimum () const
{
    return this->m_min;
}

const irr::f64 CRotationRandom::getMaximum () const
{
    return this->m_max;
}