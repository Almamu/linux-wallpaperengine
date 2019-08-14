#pragma once

#include "../initializer.h"

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

namespace wp::core::objects::particles::initializers
{
    class velocityrandom : initializer
    {
    public:
        irr::core::vector3df* getMinimum ();
        irr::core::vector3df* getMaximum ();
    protected:
        friend class initializer;

        static velocityrandom* fromJSON (json data, irr::u32 id);

        velocityrandom (irr::u32 id, irr::core::vector3df min, irr::core::vector3df max);
    private:
        irr::core::vector3df m_max;
        irr::core::vector3df m_min;
    };
};
