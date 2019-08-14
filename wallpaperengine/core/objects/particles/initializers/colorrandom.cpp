#include "colorrandom.h"

#include "../../../../core.h"

using namespace wp::core::objects::particles::initializers;

colorrandom* colorrandom::fromJSON (json data, irr::u32 id)
{
    json::const_iterator min_it = data.find ("min");
    json::const_iterator max_it = data.find ("max");

    if (min_it == data.end ())
    {
        throw std::runtime_error ("Colorrandom initializer must have a minimum value");
    }

    if (max_it == data.end ())
    {
        throw std::runtime_error ("Colorrandom initializer must have a maximum value");
    }

    return new colorrandom (
        id,
        wp::core::atoSColor (*min_it),
        wp::core::atoSColor (*max_it)
    );
}


colorrandom::colorrandom (irr::u32 id, irr::video::SColor min, irr::video::SColor max) :
    initializer (id, "colorrandom"),
    m_min (min),
    m_max (max)
{
}

irr::video::SColor* colorrandom::getMinimum ()
{
    return &this->m_min;
}

irr::video::SColor* colorrandom::getMaximum ()
{
    return &this->m_max;
}