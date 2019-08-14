#include "controlpoint.h"

#include "../../../core.h"

using namespace wp::core::objects::particles;

controlpoint* controlpoint::fromJSON (json data)
{
    json::const_iterator flags_it = data.find ("flags");
    json::const_iterator id_it = data.find ("id");
    json::const_iterator offset_it = data.find ("offset");

    if (id_it == data.end ())
    {
        throw std::runtime_error ("Particle's control point must have id");
    }

    controlpoint* controlpoint = new class controlpoint (*id_it, 0);

    if (offset_it != data.end ())
    {
        controlpoint->setOffset (wp::core::ato3vf (*offset_it));
    }

    if (flags_it != data.end ())
    {
        controlpoint->setFlags (*flags_it);
    }

    return controlpoint;
}

controlpoint::controlpoint (irr::u32 id, irr::u32 flags) :
    m_id (id),
    m_flags (flags),
    m_offset (irr::core::vector3df ())
{
}

void controlpoint::setOffset (const irr::core::vector3df& offset)
{
    this->m_offset = offset;
}

void controlpoint::setFlags (irr::u32 flags)
{
    this->m_flags = flags;
}

irr::core::vector3df* controlpoint::getOffset ()
{
    return &this->m_offset;
}

irr::u32 controlpoint::getFlags ()
{
    return this->m_flags;
}