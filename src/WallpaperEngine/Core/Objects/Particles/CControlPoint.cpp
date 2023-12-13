#include "CControlPoint.h"

using namespace WallpaperEngine::Core::Objects::Particles;

CControlPoint* CControlPoint::fromJSON (json data) {
    const auto flags_it = data.find ("flags");
    const auto id_it = jsonFindRequired (data, "id", "Particle's control point must have id");
    const auto offset_it = data.find ("offset");

    auto* controlpoint = new CControlPoint (*id_it, 0);

    if (offset_it != data.end ())
        controlpoint->setOffset (WallpaperEngine::Core::aToVector3 (*offset_it));

    if (flags_it != data.end ())
        controlpoint->setFlags (*flags_it);

    return controlpoint;
}

CControlPoint::CControlPoint (uint32_t id, uint32_t flags) : m_id (id), m_flags (flags), m_offset (glm::vec3 ()) {}

void CControlPoint::setOffset (const glm::vec3& offset) {
    this->m_offset = offset;
}

void CControlPoint::setFlags (uint32_t flags) {
    this->m_flags = flags;
}

uint32_t CControlPoint::getId () const {
    return this->m_id;
}

const glm::vec3& CControlPoint::getOffset () const {
    return this->m_offset;
}

uint32_t CControlPoint::getFlags () const {
    return this->m_flags;
}