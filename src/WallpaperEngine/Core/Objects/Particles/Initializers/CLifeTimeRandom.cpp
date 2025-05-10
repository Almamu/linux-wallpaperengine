#include "CLifeTimeRandom.h"

using namespace WallpaperEngine::Core::Objects::Particles::Initializers;

const CLifeTimeRandom* CLifeTimeRandom::fromJSON (const json& data, uint32_t id) {
    return new CLifeTimeRandom (
        id,
        jsonFindRequired <uint32_t> (data, "min", "Lifetimerandom initializer must have a minimum value"),
        jsonFindRequired <uint32_t> (data, "max", "Lifetimerandom initializer must have a maximum value")
    );
}

CLifeTimeRandom::CLifeTimeRandom (uint32_t id, uint32_t min, uint32_t max) :
    CInitializer (id, "lifetimerandom"),
    m_max (max),
    m_min (min) {}

uint32_t CLifeTimeRandom::getMinimum () const {
    return this->m_min;
}

uint32_t CLifeTimeRandom::getMaximum () const {
    return this->m_max;
}