#include "CSizeRandom.h"

using namespace WallpaperEngine::Core::Objects::Particles::Initializers;

const CSizeRandom* CSizeRandom::fromJSON (const json& data, uint32_t id) {
    return new CSizeRandom (
        id,
        jsonFindRequired <uint32_t> (data, "min", "Sizerandom initializer must have a minimum value"),
        jsonFindRequired <uint32_t> (data, "max", "Sizerandom initializer must have a maximum value")
    );
}

CSizeRandom::CSizeRandom (uint32_t id, uint32_t min, uint32_t max) :
    CInitializer (id, "sizerandom"),
    m_max (max),
    m_min (min) {}

uint32_t CSizeRandom::getMinimum () const {
    return this->m_min;
}

uint32_t CSizeRandom::getMaximum () const {
    return this->m_max;
}