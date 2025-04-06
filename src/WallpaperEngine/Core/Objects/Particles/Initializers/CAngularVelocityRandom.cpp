#include "CAngularVelocityRandom.h"

using namespace WallpaperEngine::Core::Objects::Particles::Initializers;

CAngularVelocityRandom* CAngularVelocityRandom::fromJSON (json data, uint32_t id) {
    const auto min_it = jsonFindRequired (data, "min", "Angularvelocityrandom initializer must have a minimum value");
    const auto max_it = jsonFindRequired (data, "max", "Angularvelocityrandom initializer must have a maximum value");

    return new CAngularVelocityRandom (id, WallpaperEngine::Core::aToVector3 (*min_it),
                                       WallpaperEngine::Core::aToVector3 (*max_it));
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