#include "CEmitter.h"

#include "WallpaperEngine/Core/Core.h"

using namespace WallpaperEngine::Core::Objects::Particles;

CEmitter* CEmitter::fromJSON (json data)
{
    auto directions_it = jsonFindValueRequired(&data, "directions", "Particle emitter must have direction specified");
    auto distancemax_it = jsonFindValueRequired(&data, "distancemax", "Particle emitter must have maximum distance");
    auto distancemin_it = jsonFindValueRequired(&data, "distancemin", "Particle emitter must have minimum distance");
    auto id_it = data.find ("id");
    auto name_it = jsonFindValueRequired(&data, "name", "Particle emitter must have a name");
    auto origin_it = jsonFindValueRequired(&data, "origin", "Particle emitter must have an origin");
    auto rate_it = jsonFindValueRequired(&data, "rate", "Particle emitter must have a rate");

    return new CEmitter (
            WallpaperEngine::Core::ato3vf (*directions_it),
            *distancemax_it,
            *distancemin_it,
            (id_it == data.end () ? 0 : (irr::u32) (*id_it)),
            *name_it,
            WallpaperEngine::Core::ato3vf (*origin_it),
            *rate_it
    );
}

CEmitter::CEmitter (
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


const std::string& CEmitter::getName () const
{
    return this->m_name;
}

const irr::u32 CEmitter::getDistanceMax () const
{
    return this->m_distancemax;
}

const irr::u32 CEmitter::getDistanceMin () const
{
    return this->m_distancemin;
}

const irr::core::vector3df& CEmitter::getDirections () const
{
    return this->m_directions;
}

const irr::core::vector3df& CEmitter::getOrigin () const
{
    return this->m_origin;
}

const irr::f64 CEmitter::getRate () const
{
    return this->m_rate;
}