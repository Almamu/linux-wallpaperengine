#include "CControlPoint.h"

using namespace WallpaperEngine::Core::Objects::Particles;

const CControlPoint* CControlPoint::fromJSON (const json& data) {
    return new CControlPoint (
        jsonFindRequired <uint32_t> (data, "id", "Particle's control point must have id"),
        jsonFindDefault (data, "flags", 0),
        jsonFindDefault (data, "offset", glm::vec3())
    );
}

CControlPoint::CControlPoint (uint32_t id, uint32_t flags, glm::vec3 offset) :
    m_id (id),
    m_flags (flags),
    m_offset (offset) {}

uint32_t CControlPoint::getId () const {
    return this->m_id;
}

const glm::vec3& CControlPoint::getOffset () const {
    return this->m_offset;
}

uint32_t CControlPoint::getFlags () const {
    return this->m_flags;
}