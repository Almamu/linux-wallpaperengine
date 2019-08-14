#pragma once

#include "../initializer.h"

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

namespace wp::core::objects::particles::initializers
{
    class rotationrandom : initializer
    {
    public:
        irr::u32 getMinimum ();
        irr::u32 getMaximum ();
    protected:
        friend class initializer;

        static rotationrandom* fromJSON (json data, irr::u32 id);

        rotationrandom (irr::u32 id, irr::f64 min, irr::f64 max);
    private:
        irr::f64 m_max;
        irr::f64 m_min;
    };
};
