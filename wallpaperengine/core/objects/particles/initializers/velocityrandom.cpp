#include "velocityrandom.h"

#include "../../../../core.h"

using namespace wp::core::objects::particles::initializers;

velocityrandom* velocityrandom::fromJSON (json data, irr::u32 id)
{
    json::const_iterator min_it = data.find ("min");
    json::const_iterator max_it = data.find ("max");

    if (min_it == data.end ())
    {
        throw std::runtime_error ("Velocityrandom initializer must have a minimum value");
    }

    if (max_it == data.end ())
    {
        throw std::runtime_error ("Velocityrandom initializer must have a maximum value");
    }

    return new velocityrandom (
        id,
        wp::core::ato3vf (*min_it),
        wp::core::ato3vf (*max_it)
    );
}


velocityrandom::velocityrandom (irr::u32 id, irr::core::vector3df min, irr::core::vector3df max) :
    initializer (id, "velocityrandom"),
    m_min (min),
    m_max (max)
{
}

irr::core::vector3df* velocityrandom::getMinimum ()
{
    return &this->m_min;
}

irr::core::vector3df* velocityrandom::getMaximum ()
{
    return &this->m_max;
}