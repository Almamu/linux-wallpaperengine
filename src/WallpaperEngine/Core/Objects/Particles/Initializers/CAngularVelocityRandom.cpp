#include "CAngularVelocityRandom.h"

using namespace WallpaperEngine::Core::Objects::Particles::Initializers;

const CAngularVelocityRandom* CAngularVelocityRandom::fromJSON (const json& data, uint32_t id) {
    return new CAngularVelocityRandom (
        id,
        jsonFindRequired <glm::vec3> (data, "min", "Angularvelocityrandom initializer must have a minimum value"),
        jsonFindRequired <glm::vec3> (data, "max", "Angularvelocityrandom initializer must have a maximum value")
    );
}

CAngularVelocityRandom::CAngularVelocityRandom (uint32_t id, glm::vec3 min, glm::vec3 max) :
    CInitializer (id, "angularvelocityrandom"),
    m_max (max),
    m_min (min) {}

const glm::vec3& CAngularVelocityRandom::getMinimum () const {
    return this->m_min;
}

const glm::vec3& CAngularVelocityRandom::getMaximum () const {
    return this->m_max;
}