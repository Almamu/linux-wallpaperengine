#include "CTurbulentVelocityRandom.h"

#include "WallpaperEngine/Core/Core.h"

using namespace WallpaperEngine::Core::Objects::Particles::Initializers;

CTurbulentVelocityRandom* CTurbulentVelocityRandom::fromJSON (json data, uint32_t id)
{
    json::const_iterator phasemax_it = data.find ("phasemax");
    json::const_iterator scale_it = data.find ("scale");
    json::const_iterator speedmax_it = data.find ("speedmax");
    json::const_iterator speedmin_it = data.find ("speedmin");
    json::const_iterator timescale_it = data.find ("timescale");

    if (phasemax_it == data.end ())
    {
        throw std::runtime_error ("TurbulentVelocityRandom initializer must have a phasemax value");
    }

    if (scale_it == data.end ())
    {
        throw std::runtime_error ("TurbulentVelocityRandom initializer must have a scale value");
    }

    if (speedmax_it == data.end ())
    {
        throw std::runtime_error ("TurbulentVelocityRandom initializer must have a maximum speed value");
    }

    if (speedmin_it == data.end ())
    {
        throw std::runtime_error ("TurbulentVelocityRandom initializer must have a minimum speed value");
    }

    if (timescale_it == data.end ())
    {
        throw std::runtime_error ("TurbulentVelocityRandom initializer must have a timescale value");
    }

    return new CTurbulentVelocityRandom (
            id,
            *phasemax_it,
            *scale_it,
            *timescale_it,
            *speedmin_it,
            *speedmax_it
    );
}


CTurbulentVelocityRandom::CTurbulentVelocityRandom (uint32_t id,
                          double phasemax, double scale, double timescale, uint32_t speedmin, uint32_t speedmax) :
        CInitializer (id, "turbulentvelocityrandom"),
        m_phasemax (phasemax),
        m_scale (scale),
        m_timescale (timescale),
        m_speedmin (speedmin),
        m_speedmax (speedmax)
{
}


double CTurbulentVelocityRandom::getPhaseMax ()
{
    return this->m_phasemax;
}

double CTurbulentVelocityRandom::getScale ()
{
    return this->m_scale;
}

double CTurbulentVelocityRandom::getTimeScale ()
{
    return this->m_timescale;
}

uint32_t CTurbulentVelocityRandom::getMinimumSpeed ()
{
    return this->m_speedmin;
}

uint32_t CTurbulentVelocityRandom::getMaximumSpeed ()
{
    return this->m_speedmax;
}
