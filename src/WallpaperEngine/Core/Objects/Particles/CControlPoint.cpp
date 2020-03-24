#include "CControlPoint.h"

#include "WallpaperEngine/Core/Core.h"

using namespace WallpaperEngine::Core::Objects::Particles;

CControlPoint* CControlPoint::fromJSON (json data)
{
    auto flags_it = data.find ("flags");
    auto id_it = jsonFindRequired (&data, "id", "Particle's control point must have id");
    auto offset_it = data.find ("offset");

    CControlPoint* controlpoint = new CControlPoint (*id_it, 0);

    if (offset_it != data.end ())
    {
        controlpoint->setOffset (WallpaperEngine::Core::ato3vf (*offset_it));
    }

    if (flags_it != data.end ())
    {
        controlpoint->setFlags (*flags_it);
    }

    return controlpoint;
}

CControlPoint::CControlPoint (irr::u32 id, irr::u32 flags) :
    m_id (id),
    m_flags (flags),
    m_offset (irr::core::vector3df ())
{
}

void CControlPoint::setOffset (const irr::core::vector3df& offset)
{
    this->m_offset = offset;
}

void CControlPoint::setFlags (irr::u32 flags)
{
    this->m_flags = flags;
}

const irr::core::vector3df& CControlPoint::getOffset () const
{
    return this->m_offset;
}

const irr::u32 CControlPoint::getFlags () const
{
    return this->m_flags;
}