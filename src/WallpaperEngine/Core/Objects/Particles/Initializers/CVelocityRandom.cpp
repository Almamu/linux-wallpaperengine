#include "CVelocityRandom.h"

using namespace WallpaperEngine::Core::Objects::Particles::Initializers;

const CVelocityRandom* CVelocityRandom::fromJSON (const json& data, uint32_t id) {
    return new CVelocityRandom (
        id,
        jsonFindRequired <glm::vec3> (data, "min", "Velocityrandom initializer must have a minimum value"),
        jsonFindRequired <glm::vec3> (data, "max", "Velocityrandom initializer must have a maximum value")
    );
}

CVelocityRandom::CVelocityRandom (uint32_t id, glm::vec3 min, glm::vec3 max) :
    CInitializer (id, "velocityrandom"),
    m_max (max),
    m_min (min) {}

const glm::vec3& CVelocityRandom::getMinimum () const {
    return this->m_min;
}

const glm::vec3& CVelocityRandom::getMaximum () const {
    return this->m_max;
}