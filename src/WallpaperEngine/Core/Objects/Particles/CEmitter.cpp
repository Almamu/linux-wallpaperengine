#include "CEmitter.h"

using namespace WallpaperEngine::Core::Objects::Particles;

CEmitter* CEmitter::fromJSON (json data)
{
    auto directions_it = jsonFindRequired (data, "directions", "Particle emitter must have direction specified");
    auto distancemax_it = jsonFindRequired (data, "distancemax", "Particle emitter must have maximum distance");
    auto distancemin_it = jsonFindRequired (data, "distancemin", "Particle emitter must have minimum distance");
    auto id_it = data.find ("id");
    auto name_it = jsonFindRequired (data, "name", "Particle emitter must have a name");
    auto origin_it = jsonFindRequired (data, "origin", "Particle emitter must have an origin");
    auto rate_it = jsonFindRequired (data, "rate", "Particle emitter must have a rate");

    return new CEmitter (
            WallpaperEngine::Core::aToVector3 (*directions_it),
            *distancemax_it,
            *distancemin_it,
            (id_it == data.end () ? 0 : (uint32_t) (*id_it)),
            *name_it,
            WallpaperEngine::Core::aToVector3 (*origin_it),
            *rate_it
    );
}

CEmitter::CEmitter (
        const glm::vec3& directions,
        uint32_t distancemax,
        uint32_t distancemin,
        uint32_t id,
        std::string name,
        const glm::vec3& origin,
        double rate):
    m_directions (directions),
    m_distancemax (distancemax),
    m_distancemin (distancemin),
    m_id (id),
    m_name (std::move(name)),
    m_origin (origin),
    m_rate (rate)
{
}


const std::string& CEmitter::getName () const
{
    return this->m_name;
}

const uint32_t CEmitter::getDistanceMax () const
{
    return this->m_distancemax;
}

const uint32_t CEmitter::getDistanceMin () const
{
    return this->m_distancemin;
}

const glm::vec3& CEmitter::getDirections () const
{
    return this->m_directions;
}

const glm::vec3& CEmitter::getOrigin () const
{
    return this->m_origin;
}

const double CEmitter::getRate () const
{
    return this->m_rate;
}