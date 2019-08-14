#include "rotationrandom.h"

using namespace wp::core::objects::particles::initializers;

rotationrandom* rotationrandom::fromJSON (json data, irr::u32 id)
{
    json::const_iterator min_it = data.find ("min");
    json::const_iterator max_it = data.find ("max");

    if (min_it == data.end ())
    {
        throw std::runtime_error ("Rotationrandom initializer must have a minimum value");
    }

    if (max_it == data.end ())
    {
        throw std::runtime_error ("Rotationrandom initializer must have a maximum value");
    }

    return new rotationrandom (id, *min_it, *max_it);
}

rotationrandom::rotationrandom (irr::u32 id, irr::f64 min, irr::f64 max) :
    initializer (id, "rotationrandom"),
    m_min (min),
    m_max (max)
{
}

irr::f64 rotationrandom::getMinimum ()
{
    return this->m_min;
}

irr::f64 rotationrandom::getMaximum ()
{
    return this->m_max;
}