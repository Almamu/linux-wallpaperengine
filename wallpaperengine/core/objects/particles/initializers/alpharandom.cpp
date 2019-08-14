#include "alpharandom.h"

using namespace wp::core::objects::particles::initializers;

alpharandom* alpharandom::fromJSON (json data, irr::u32 id)
{
    json::const_iterator min_it = data.find ("min");
    json::const_iterator max_it = data.find ("max");

    if (min_it == data.end ())
    {
        throw std::runtime_error ("Alpharandom initializer must have a minimum value");
    }

    if (max_it == data.end ())
    {
        throw std::runtime_error ("Alpharandom initializer must have a maximum value");
    }

    return new alpharandom (id, *min_it, *max_it);
}

alpharandom::alpharandom (irr::u32 id, irr::f64 min, irr::f64 max) :
    initializer (id, "alpharandom"),
    m_min (min),
    m_max (max)
{
}

irr::f64 alpharandom::getMinimum ()
{
    return this->m_min;
}

irr::f64 alpharandom::getMaximum ()
{
    return this->m_max;
}