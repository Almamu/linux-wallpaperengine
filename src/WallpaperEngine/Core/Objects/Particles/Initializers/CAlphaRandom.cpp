#include "CAlphaRandom.h"

using namespace WallpaperEngine::Core::Objects::Particles::Initializers;

CAlphaRandom* CAlphaRandom::fromJSON (json data, uint32_t id)
{
    auto min_it = jsonFindRequired (data, "min", "Alpharandom initializer must have a minimum value");
    auto max_it = jsonFindRequired (data, "max", "Alpharandom initializer must have a maximum value");

    return new CAlphaRandom (id, *min_it, *max_it);
}

CAlphaRandom::CAlphaRandom (uint32_t id, double min, double max) :
        CInitializer (id, "alpharandom"),
        m_min (min),
        m_max (max)
{
}

const double CAlphaRandom::getMinimum () const
{
    return this->m_min;
}

const double CAlphaRandom::getMaximum () const
{
    return this->m_max;
}