#include "CColorRandom.h"

using namespace WallpaperEngine::Core::Objects::Particles::Initializers;

CColorRandom* CColorRandom::fromJSON (json data, uint32_t id)
{
    auto min_it = jsonFindRequired (data, "min", "Colorrandom initializer must have a minimum value");
    auto max_it = jsonFindRequired (data, "max", "Colorrandom initializer must have a maximum value");

    return new CColorRandom (
            id,
            WallpaperEngine::Core::aToColori (*min_it),
            WallpaperEngine::Core::aToColori (*max_it)
    );
}


CColorRandom::CColorRandom (uint32_t id, glm::ivec3 min, glm::ivec3 max) :
        CInitializer (id, "colorrandom"),
        m_min (min),
        m_max (max)
{
}

const glm::ivec3& CColorRandom::getMinimum () const
{
    return this->m_min;
}

const glm::ivec3& CColorRandom::getMaximum () const
{
    return this->m_max;
}