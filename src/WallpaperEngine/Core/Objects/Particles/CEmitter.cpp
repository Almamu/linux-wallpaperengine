#include "CEmitter.h"

using namespace WallpaperEngine::Core::Objects::Particles;

const CEmitter* CEmitter::fromJSON (const json& data) {
    const auto distancemax_it = jsonFindRequired (data, "distancemax", "Particle emitter must have maximum distance");
    const auto distancemin_it = jsonFindRequired (data, "distancemin", "Particle emitter must have minimum distance");

    auto distancemin = glm::vec3(0);
    auto distancemax = glm::vec3(0);

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

    return new CEmitter (
        jsonFindRequired <glm::vec3> (data, "directions", "Particle emitter must have direction specified"),
        distancemax,
        distancemin,
        jsonFindDefault (data, "id", 0),
        jsonFindRequired <std::string> (data, "name", "Particle emitter must have a name"),
        jsonFindRequired <glm::vec3> (data, "origin", "Particle emitter must have an origin"),
        jsonFindRequired <double> (data, "rate", "Particle emitter must have a rate")
    );
}

CEmitter::CEmitter (
    glm::vec3 directions, glm::vec3 distancemax, glm::vec3 distancemin, uint32_t id, std::string name, glm::vec3 origin,
    double rate
) :
    m_directions (directions),
    m_distancemax (distancemax),
    m_distancemin (distancemin),
    m_id (id),
    m_name (name),
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

double CEmitter::getRate () const {
    return this->m_rate;
}