#include "CRotationRandom.h"

using namespace wp::core::Objects::Particles::Initializers;

CRotationRandom* CRotationRandom::fromJSON (json data, irr::u32 id)
{
    json::const_iterator min_it = data.find ("min");
    json::const_iterator max_it = data.find ("max");

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

irr::f64 CRotationRandom::getMinimum ()
{
    return this->m_min;
}

irr::f64 CRotationRandom::getMaximum ()
{
    return this->m_max;
}