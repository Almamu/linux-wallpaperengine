#pragma once

#include "../initializer.h"

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

namespace wp::core::objects::particles::initializers
{
    class colorrandom : initializer
    {
    public:
        irr::video::SColor* getMinimum ();
        irr::video::SColor* getMaximum ();
    protected:
        friend class initializer;

        static colorrandom* fromJSON (json data, irr::u32 id);

        colorrandom (irr::u32 id, irr::video::SColor min, irr::video::SColor max);
    private:
        irr::video::SColor m_max;
        irr::video::SColor m_min;
    };
};
