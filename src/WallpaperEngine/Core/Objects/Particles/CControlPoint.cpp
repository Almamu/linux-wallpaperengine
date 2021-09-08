#include "CControlPoint.h"

using namespace WallpaperEngine::Core::Objects::Particles;

CControlPoint* CControlPoint::fromJSON (json data)
{
    auto flags_it = data.find ("flags");
    auto id_it = jsonFindRequired (data, "id", "Particle's control point must have id");
    auto offset_it = data.find ("offset");

    CControlPoint* controlpoint = new CControlPoint (*id_it, 0);

    if (offset_it != data.end ())
    {
        controlpoint->setOffset (WallpaperEngine::Core::aToVector3 (*offset_it));
    }

    if (flags_it != data.end ())
    {
        controlpoint->setFlags (*flags_it);
    }

    return controlpoint;
}

CControlPoint::CControlPoint (uint32_t id, uint32_t flags) :
    m_id (id),
    m_flags (flags),
    m_offset (glm::vec3 ())
{
}

void CControlPoint::setOffset (const glm::vec3& offset)
{
    this->m_offset = offset;
}

void CControlPoint::setFlags (uint32_t flags)
{
    this->m_flags = flags;
}

const glm::vec3& CControlPoint::getOffset () const
{
    return this->m_offset;
}

const uint32_t CControlPoint::getFlags () const
{
    return this->m_flags;
}