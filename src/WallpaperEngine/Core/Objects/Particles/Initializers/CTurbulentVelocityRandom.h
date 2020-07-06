#pragma once

#include "WallpaperEngine/Core/Objects/Particles/CInitializer.h"

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

namespace WallpaperEngine::Core::Objects::Particles::Initializers
{
    class CTurbulentVelocityRandom : CInitializer
    {
    public:
        irr::f64 getPhaseMax ();
        irr::f64 getScale ();
        irr::f64 getTimeScale ();
        irr::u32 getMinimumSpeed ();
        irr::u32 getMaximumSpeed ();

    protected:
        friend class CInitializer;

        static CTurbulentVelocityRandom* fromJSON (json data, irr::u32 id);

        CTurbulentVelocityRandom (irr::u32 id,
                irr::f64 phasemax, irr::f64 scale, irr::f64 timescale, irr::u32 speedmin, irr::u32 speedmax);
    private:
        irr::f64 m_phasemax;
        irr::f64 m_scale;
        irr::f64 m_timescale;
        irr::u32 m_speedmin;
        irr::u32 m_speedmax;
    };
};
