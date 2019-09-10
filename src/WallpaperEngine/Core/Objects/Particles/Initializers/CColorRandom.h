#pragma once

#include "WallpaperEngine/Core/Objects/Particles/CInitializer.h"

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

namespace WallpaperEngine::Core::Objects::Particles::Initializers
{
    class CColorRandom : CInitializer
    {
    public:
        const irr::video::SColor& getMinimum () const;
        const irr::video::SColor& getMaximum () const;
    protected:
        friend class CInitializer;

        static CColorRandom* fromJSON (json data, irr::u32 id);

        CColorRandom (irr::u32 id, irr::video::SColor min, irr::video::SColor max);
    private:
        irr::video::SColor m_max;
        irr::video::SColor m_min;
    };
};
