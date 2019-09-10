#include "CColorRandom.h"

#include "WallpaperEngine/Core/Core.h"

using namespace WallpaperEngine::Core::Objects::Particles::Initializers;

CColorRandom* CColorRandom::fromJSON (json data, irr::u32 id)
{
    auto min_it = data.find ("min");
    auto max_it = data.find ("max");

    if (min_it == data.end ())
    {
        throw std::runtime_error ("Colorrandom initializer must have a minimum value");
    }

    if (max_it == data.end ())
    {
        throw std::runtime_error ("Colorrandom initializer must have a maximum value");
    }

    return new CColorRandom (
            id,
            WallpaperEngine::Core::atoSColor (*min_it),
            WallpaperEngine::Core::atoSColor (*max_it)
    );
}


CColorRandom::CColorRandom (irr::u32 id, irr::video::SColor min, irr::video::SColor max) :
        CInitializer (id, "colorrandom"),
        m_min (min),
        m_max (max)
{
}

const irr::video::SColor& CColorRandom::getMinimum () const
{
    return this->m_min;
}

const irr::video::SColor& CColorRandom::getMaximum () const
{
    return this->m_max;
}