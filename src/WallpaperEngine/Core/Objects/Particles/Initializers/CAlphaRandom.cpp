#include "CAlphaRandom.h"

using namespace WallpaperEngine::Core::Objects::Particles::Initializers;

const CAlphaRandom* CAlphaRandom::fromJSON (const json& data, uint32_t id) {
    return new CAlphaRandom (
        id,
        jsonFindRequired<double> (data, "min", "Alpharandom initializer must have a minimum value"),
        jsonFindRequired<double> (data, "max", "Alpharandom initializer must have a maximum value")
    );
}

CAlphaRandom::CAlphaRandom (uint32_t id, double min, double max) :
    CInitializer (id, "alpharandom"),
    m_max (max),
    m_min (min) {}

double CAlphaRandom::getMinimum () const {
    return this->m_min;
}

double CAlphaRandom::getMaximum () const {
    return this->m_max;
}