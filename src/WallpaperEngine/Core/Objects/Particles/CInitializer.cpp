#include "CInitializer.h"

#include "WallpaperEngine/Core/Objects/Particles/Initializers/CAlphaRandom.h"
#include "WallpaperEngine/Core/Objects/Particles/Initializers/CAngularVelocityRandom.h"
#include "WallpaperEngine/Core/Objects/Particles/Initializers/CColorRandom.h"
#include "WallpaperEngine/Core/Objects/Particles/Initializers/CLifeTimeRandom.h"
#include "WallpaperEngine/Core/Objects/Particles/Initializers/CRotationRandom.h"
#include "WallpaperEngine/Core/Objects/Particles/Initializers/CSizeRandom.h"
#include "WallpaperEngine/Core/Objects/Particles/Initializers/CTurbulentVelocityRandom.h"
#include "WallpaperEngine/Core/Objects/Particles/Initializers/CVelocityRandom.h"
#include "WallpaperEngine/Logging/CLog.h"

using namespace WallpaperEngine::Core::Objects::Particles;

const CInitializer* CInitializer::fromJSON (const json& data) {
    const auto name = jsonFindRequired <std::string> (data, "name", "Particle's initializer must have a name");
    const auto id = jsonFindDefault (data, "id", 0);

    if (name == "lifetimerandom") {
        return Initializers::CLifeTimeRandom::fromJSON (data, id);
    }
    if (name == "sizerandom") {
        return Initializers::CSizeRandom::fromJSON (data, id);
    }
    if (name == "rotationrandom") {
        return Initializers::CRotationRandom::fromJSON (data, id);
    }
    if (name == "velocityrandom") {
        return Initializers::CVelocityRandom::fromJSON (data, id);
    }
    if (name == "colorrandom") {
        return Initializers::CColorRandom::fromJSON (data, id);
    }
    if (name == "alpharandom") {
        return Initializers::CAlphaRandom::fromJSON (data, id);
    }
    if (name == "angularvelocityrandom") {
        return Initializers::CAngularVelocityRandom::fromJSON (data, id);
    }
    if (name == "turbulentvelocityrandom") {
        return Initializers::CTurbulentVelocityRandom::fromJSON (data, id);
    }

    sLog.exception ("Found unknown initializer for particles: ", name);
}

CInitializer::CInitializer (uint32_t id, std::string name) :
    m_id (id),
    m_name (name) {}

const std::string& CInitializer::getName () const {
    return this->m_name;
}

uint32_t CInitializer::getId () const {
    return this->m_id;
}