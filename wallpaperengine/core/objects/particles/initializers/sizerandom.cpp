#include "sizerandom.h"

using namespace wp::core::objects::particles::initializers;

sizerandom* sizerandom::fromJSON (json data, irr::u32 id)
{
    json::const_iterator min_it = data.find ("min");
    json::const_iterator max_it = data.find ("max");

    if (min_it == data.end ())
    {
        throw std::runtime_error ("Sizerandom initializer must have a minimum value");
    }

    if (max_it == data.end ())
    {
        throw std::runtime_error ("Sizerandom initializer must have a maximum value");
    }

    return new sizerandom (id, *min_it, *max_it);
}

sizerandom::sizerandom (irr::u32 id, irr::u32 min, irr::u32 max) :
    initializer (id, "sizerandom"),
    m_min (min),
    m_max (max)
{
}

irr::u32 sizerandom::getMinimum ()
{
    return this->m_min;
}

irr::u32 sizerandom::getMaximum ()
{
    return this->m_max;
}