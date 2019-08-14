#include "CColorRandom.h"

#include "../../../../core.h"

using namespace wp::core::Objects::Particles::Initializers;

CColorRandom* CColorRandom::fromJSON (json data, irr::u32 id)
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

    return new CColorRandom (
        id,
        wp::core::atoSColor (*min_it),
        wp::core::atoSColor (*max_it)
    );
}


CColorRandom::CColorRandom (irr::u32 id, irr::video::SColor min, irr::video::SColor max) :
        CInitializer (id, "colorrandom"),
        m_min (min),
        m_max (max)
{
}

irr::video::SColor* CColorRandom::getMinimum ()
{
    return &this->m_min;
}

irr::video::SColor* CColorRandom::getMaximum ()
{
    return &this->m_max;
}