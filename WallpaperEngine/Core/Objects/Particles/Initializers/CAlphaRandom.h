#pragma once

#include "../CInitializer.h"

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

namespace WallpaperEngine::Core::Objects::Particles::Initializers
{
    class CAlphaRandom : CInitializer
    {
    public:
        irr::f64 getMinimum ();
        irr::f64 getMaximum ();
    protected:
        friend class CInitializer;

        static CAlphaRandom* fromJSON (json data, irr::u32 id);

        CAlphaRandom (irr::u32 id, irr::f64 min, irr::f64 max);
    private:
        irr::f64 m_max;
        irr::f64 m_min;
    };
};
