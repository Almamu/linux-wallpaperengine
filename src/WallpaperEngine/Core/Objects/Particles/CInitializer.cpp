#include "CInitializer.h"

#include "WallpaperEngine/Core/Objects/Particles/Initializers/CLifeTimeRandom.h"
#include "WallpaperEngine/Core/Objects/Particles/Initializers/CSizeRandom.h"
#include "WallpaperEngine/Core/Objects/Particles/Initializers/CRotationRandom.h"
#include "WallpaperEngine/Core/Objects/Particles/Initializers/CVelocityRandom.h"
#include "WallpaperEngine/Core/Objects/Particles/Initializers/CColorRandom.h"
#include "WallpaperEngine/Core/Objects/Particles/Initializers/CAlphaRandom.h"
#include "WallpaperEngine/Core/Objects/Particles/Initializers/CAngularVelocityRandom.h"
#include "WallpaperEngine/Core/Objects/Particles/Initializers/CTurbulentVelocityRandom.h"

using namespace WallpaperEngine::Core::Objects::Particles;

CInitializer* CInitializer::fromJSON (json data)
{
    auto id_it = data.find ("id");
    auto name_it = jsonFindRequired (data, "name", "Particle's initializer must have a name");
    uint32_t id = ((id_it == data.end ()) ? 0 : (uint32_t) (*id_it));

    if (*name_it == "lifetimerandom")
    {
        return Initializers::CLifeTimeRandom::fromJSON (data, id);
    }
    else if (*name_it == "sizerandom")
    {
        return Initializers::CSizeRandom::fromJSON (data, id);
    }
    else if (*name_it == "rotationrandom")
    {
        return Initializers::CRotationRandom::fromJSON (data, id);
    }
    else if (*name_it == "velocityrandom")
    {
        return Initializers::CVelocityRandom::fromJSON (data, id);
    }
    else if (*name_it == "colorrandom")
    {
        return Initializers::CColorRandom::fromJSON (data, id);
    }
    else if (*name_it == "alpharandom")
    {
        return Initializers::CAlphaRandom::fromJSON (data, id);
    }
    else if (*name_it == "angularvelocityrandom")
    {
        return Initializers::CAngularVelocityRandom::fromJSON (data, id);
    }
    else if (*name_it == "turbulentvelocityrandom")
    {
        return Initializers::CTurbulentVelocityRandom::fromJSON (data, id);
    }
    else
    {
        throw std::runtime_error ("Particle's got an unknown initializer");
    }
}


CInitializer::CInitializer (uint32_t id, std::string name) :
    m_id (id),
    m_name (std::move(name))
{
}


const std::string& CInitializer::getName () const
{
    return this->m_name;
}

const uint32_t CInitializer::getId () const
{
    return this->m_id;
}