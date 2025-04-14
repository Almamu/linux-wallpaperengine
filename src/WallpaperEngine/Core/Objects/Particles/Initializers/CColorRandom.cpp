#include "CColorRandom.h"

using namespace WallpaperEngine::Core::Objects::Particles::Initializers;

const CColorRandom* CColorRandom::fromJSON (const json& data, uint32_t id) {
    return new CColorRandom (
        id,
        jsonFindRequired <glm::ivec3> (data, "min", "Colorrandom initializer must have a minimum value"),
        jsonFindRequired <glm::ivec3> (data, "max", "Colorrandom initializer must have a maximum value")
    );
}

CColorRandom::CColorRandom (uint32_t id, glm::ivec3 min, glm::ivec3 max) :
    CInitializer (id, "colorrandom"),
    m_max (max),
    m_min (min) {}

const glm::ivec3& CColorRandom::getMinimum () const {
    return this->m_min;
}

const glm::ivec3& CColorRandom::getMaximum () const {
    return this->m_max;
}