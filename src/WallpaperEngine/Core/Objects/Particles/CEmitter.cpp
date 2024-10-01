#include "CEmitter.h"

using namespace WallpaperEngine::Core::Objects::Particles;

CEmitter* CEmitter::fromJSON (json data) {
    const auto directions_it = jsonFindRequired (data, "directions", "Particle emitter must have direction specified");
    const auto distancemax_it = jsonFindRequired (data, "distancemax", "Particle emitter must have maximum distance");
    const auto distancemin_it = jsonFindRequired (data, "distancemin", "Particle emitter must have minimum distance");
    const auto id_it = data.find ("id");
    const auto name_it = jsonFindRequired (data, "name", "Particle emitter must have a name");
    const auto origin_it = jsonFindRequired (data, "origin", "Particle emitter must have an origin");
    const auto rate_it = jsonFindRequired (data, "rate", "Particle emitter must have a rate");

    glm::vec3 distancemin = glm::vec3(0);
    glm::vec3 distancemax = glm::vec3(0);

    if (distancemin_it->is_number()) {
        distancemin = glm::vec3(static_cast<uint32_t>(*distancemin_it));
    } else {
        distancemin = WallpaperEngine::Core::aToVector3(*distancemin_it);
    }

    if (distancemax_it->is_number()) {
        distancemax = glm::vec3(static_cast<uint32_t>(*distancemax_it));
    } else {
        distancemax = WallpaperEngine::Core::aToVector3(*distancemax_it);
    }

    return new CEmitter (WallpaperEngine::Core::aToVector3 (*directions_it), distancemax, distancemin,
                         (id_it == data.end () ? 0 : static_cast<uint32_t> (*id_it)), *name_it,
                         WallpaperEngine::Core::aToVector3 (*origin_it), *rate_it);
}

CEmitter::CEmitter (const glm::vec3& directions, const glm::vec3& distancemax, const glm::vec3& distancemin, uint32_t id,
                    std::string name, const glm::vec3& origin, double rate) :
    m_directions (directions),
    m_distancemax (distancemax),
    m_distancemin (distancemin),
    m_id (id),
    m_name (std::move (name)),
    m_origin (origin),
    m_rate (rate) {}

uint32_t CEmitter::getId () const {
    return this->m_id;
}

const std::string& CEmitter::getName () const {
    return this->m_name;
}

const glm::vec3& CEmitter::getDistanceMax () const {
    return this->m_distancemax;
}

const glm::vec3& CEmitter::getDistanceMin () const {
    return this->m_distancemin;
}

const glm::vec3& CEmitter::getDirections () const {
    return this->m_directions;
}

const glm::vec3& CEmitter::getOrigin () const {
    return this->m_origin;
}

const double CEmitter::getRate () const {
    return this->m_rate;
}