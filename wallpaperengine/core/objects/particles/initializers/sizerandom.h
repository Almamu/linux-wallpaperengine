#pragma once

#include "../initializer.h"

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

namespace wp::core::objects::particles::initializers
{
    class sizerandom : initializer
    {
    public:
        irr::u32 getMinimum ();
        irr::u32 getMaximum ();
    protected:
        friend class initializer;

        static sizerandom* fromJSON (json data, irr::u32 id);

        sizerandom (irr::u32 id, irr::u32 min, irr::u32 max);
    private:
        irr::u32 m_max;
        irr::u32 m_min;
    };
};
