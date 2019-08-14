#pragma once

#include "../CInitializer.h"

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

namespace wp::core::Objects::Particles::Initializers
{
    class CAngularVelocityRandom : CInitializer
    {
    public:
        irr::core::vector3df* getMinimum ();
        irr::core::vector3df* getMaximum ();
    protected:
        friend class CInitializer;

        static CAngularVelocityRandom* fromJSON (json data, irr::u32 id);

        CAngularVelocityRandom (irr::u32 id, irr::core::vector3df min, irr::core::vector3df max);
    private:
        irr::core::vector3df m_max;
        irr::core::vector3df m_min;
    };
};
