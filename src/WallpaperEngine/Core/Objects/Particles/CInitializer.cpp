#include "CInitializer.h"
#include "common.h"

#include "WallpaperEngine/Core/Objects/Particles/Initializers/CAlphaRandom.h"
#include "WallpaperEngine/Core/Objects/Particles/Initializers/CAngularVelocityRandom.h"
#include "WallpaperEngine/Core/Objects/Particles/Initializers/CColorRandom.h"
#include "WallpaperEngine/Core/Objects/Particles/Initializers/CLifeTimeRandom.h"
#include "WallpaperEngine/Core/Objects/Particles/Initializers/CRotationRandom.h"
#include "WallpaperEngine/Core/Objects/Particles/Initializers/CSizeRandom.h"
#include "WallpaperEngine/Core/Objects/Particles/Initializers/CTurbulentVelocityRandom.h"
#include "WallpaperEngine/Core/Objects/Particles/Initializers/CVelocityRandom.h"

using namespace WallpaperEngine::Core::Objects::Particles;

CInitializer* CInitializer::fromJSON (json data) {
    const auto id_it = data.find ("id");
    const auto name_it = jsonFindRequired (data, "name", "Particle's initializer must have a name");
    const uint32_t id = ((id_it == data.end ()) ? 0 : static_cast<uint32_t> (*id_it));

    if (*name_it == "lifetimerandom") {
        return Initializers::CLifeTimeRandom::fromJSON (data, id);
    }
    if (*name_it == "sizerandom") {
        return Initializers::CSizeRandom::fromJSON (data, id);
    }
    if (*name_it == "rotationrandom") {
        return Initializers::CRotationRandom::fromJSON (data, id);
    }
    if (*name_it == "velocityrandom") {
        return Initializers::CVelocityRandom::fromJSON (data, id);
    }
    if (*name_it == "colorrandom") {
        return Initializers::CColorRandom::fromJSON (data, id);
    }
    if (*name_it == "alpharandom") {
        return Initializers::CAlphaRandom::fromJSON (data, id);
    }
    if (*name_it == "angularvelocityrandom") {
        return Initializers::CAngularVelocityRandom::fromJSON (data, id);
    }
    if (*name_it == "turbulentvelocityrandom") {
        return Initializers::CTurbulentVelocityRandom::fromJSON (data, id);
    }

    sLog.exception ("Found unknown initializer for particles: ", *name_it);
}

CInitializer::CInitializer (uint32_t id, std::string name) : m_id (id), m_name (std::move (name)) {}

const std::string& CInitializer::getName () const {
    return this->m_name;
}

uint32_t CInitializer::getId () const {
    return this->m_id;
}