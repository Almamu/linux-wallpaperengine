#include "emitter.h"

#include "../../../core.h"

using namespace wp::core::objects::particles;

emitter* emitter::fromJSON (json data)
{
    json::const_iterator directions_it = data.find ("directions");
    json::const_iterator distancemax_it = data.find ("distancemax");
    json::const_iterator distancemin_it = data.find ("distancemin");
    json::const_iterator id_it = data.find ("id");
    json::const_iterator name_it = data.find ("name");
    json::const_iterator origin_it = data.find ("origin");
    json::const_iterator rate_it = data.find ("rate");

    if (directions_it == data.end ())
    {
        throw std::runtime_error ("Particle emitter must have direction specified");
    }

    if (distancemax_it == data.end ())
    {
        throw std::runtime_error ("Particle emitter must have maximum distance");
    }

    if (distancemin_it == data.end ())
    {
        throw std::runtime_error ("Particle emitter must have minimum distance");
    }

    if (id_it == data.end ())
    {
        throw std::runtime_error ("Particle emitter must have an id");
    }

    if (name_it == data.end ())
    {
        throw std::runtime_error ("Particle emitter must have a name");
    }

    if (origin_it == data.end ())
    {
        throw std::runtime_error ("Particle emitter must have an origin");
    }

    if (rate_it == data.end ())
    {
        throw std::runtime_error ("Particle emitter must have a rate");
    }

    return new emitter (
        wp::core::ato3vf (*directions_it),
        *distancemax_it,
        *distancemin_it,
        *id_it,
        *name_it,
        wp::core::ato3vf (*origin_it),
        *rate_it
    );
}

emitter::emitter (
        const irr::core::vector3df& directions,
        irr::u32 distancemax,
        irr::u32 distancemin,
        irr::u32 id,
        std::string name,
        const irr::core::vector3df& origin,
        irr::f64 rate):
    m_directions (directions),
    m_distancemax (distancemax),
    m_distancemin (distancemin),
    m_id (id),
    m_name (std::move(name)),
    m_origin (origin),
    m_rate (rate)
{
}


const std::string& emitter::getName () const
{
    return this->m_name;
}

irr::u32 emitter::getDistanceMax () const
{
    return this->m_distancemax;
}

irr::u32 emitter::getDistanceMin () const
{
    return this->m_distancemin;
}

irr::core::vector3df* emitter::getDirections ()
{
    return &this->m_directions;
}

irr::core::vector3df* emitter::getOrigin ()
{
    return &this->m_origin;
}

irr::f64 emitter::getRate () const
{
    return this->m_rate;
}