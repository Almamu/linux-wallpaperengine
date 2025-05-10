#include "CTurbulentVelocityRandom.h"

#include "WallpaperEngine/Core/Core.h"
#include "WallpaperEngine/Logging/CLog.h"

using namespace WallpaperEngine::Core::Objects::Particles::Initializers;

const CTurbulentVelocityRandom* CTurbulentVelocityRandom::fromJSON (const json& data, uint32_t id) {
    return new CTurbulentVelocityRandom (
        id,
        jsonFindRequired <double> (data, "phasemax", "TurbulentVelocityRandom initializer must have a phasemax value"),
        jsonFindRequired <double> (data, "scale", "TurbulentVelocityRandom initializer must have a scale value"),
        jsonFindRequired <double>(data, "timescale", "TurbulentVelocityRandom initializer must have a timescale value"),
        jsonFindRequired <uint32_t> (data, "speedmin", "TurbulentVelocityRandom initializer must have a minimum speed value"),
        jsonFindRequired <uint32_t> (data, "speedmax", "TurbulentVelocityRandom initializer must have a maximum speed value")
    );
}

CTurbulentVelocityRandom::CTurbulentVelocityRandom (uint32_t id, double phasemax, double scale, double timescale,
                                                    uint32_t speedmin, uint32_t speedmax) :
    CInitializer (id, "turbulentvelocityrandom"),
    m_phasemax (phasemax),
    m_scale (scale),
    m_timescale (timescale),
    m_speedmin (speedmin),
    m_speedmax (speedmax) {}

double CTurbulentVelocityRandom::getPhaseMax () const {
    return this->m_phasemax;
}

double CTurbulentVelocityRandom::getScale () const {
    return this->m_scale;
}

double CTurbulentVelocityRandom::getTimeScale () const {
    return this->m_timescale;
}

uint32_t CTurbulentVelocityRandom::getMinimumSpeed () const {
    return this->m_speedmin;
}

uint32_t CTurbulentVelocityRandom::getMaximumSpeed () const {
    return this->m_speedmax;
}
