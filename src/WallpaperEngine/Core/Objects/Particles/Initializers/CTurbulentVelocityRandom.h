#pragma once

#include "WallpaperEngine/Core/Objects/Particles/CInitializer.h"

#include <nlohmann/json.hpp>

namespace WallpaperEngine::Core::Objects::Particles::Initializers
{
    class CTurbulentVelocityRandom : CInitializer
    {
    public:
        double getPhaseMax ();
        double getScale ();
        double getTimeScale ();
        uint32_t getMinimumSpeed ();
        uint32_t getMaximumSpeed ();

    protected:
        friend class CInitializer;

        static CTurbulentVelocityRandom* fromJSON (json data, uint32_t id);

        CTurbulentVelocityRandom (uint32_t id,
                double phasemax, double scale, double timescale, uint32_t speedmin, uint32_t speedmax);
    private:
        double m_phasemax;
        double m_scale;
        double m_timescale;
        uint32_t m_speedmin;
        uint32_t m_speedmax;
    };
};
