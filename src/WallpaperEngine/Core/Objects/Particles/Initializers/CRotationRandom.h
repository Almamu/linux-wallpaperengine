#pragma once

#include "WallpaperEngine/Core/Objects/Particles/CInitializer.h"

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

namespace WallpaperEngine::Core::Objects::Particles::Initializers
{
    class CRotationRandom : CInitializer
    {
    public:
        const irr::f64 getMinimum () const;
        const irr::f64 getMaximum () const;
    protected:
        friend class CInitializer;

        static CRotationRandom* fromJSON (json data, irr::u32 id);

        CRotationRandom (irr::u32 id, irr::f64 min, irr::f64 max);
    private:
        irr::f64 m_max;
        irr::f64 m_min;
    };
};