#ifndef WALLENGINE_CORE_H
#define WALLENGINE_CORE_H

#include <iostream>
#include <irrlicht/vector3d.h>
#include <irrlicht/vector2d.h>

namespace wp
{

    class core
    {
    public:
        static irr::core::vector3df ato3vf (const char *str);
        static irr::core::vector2df ato2vf (const char *str);
    };
};

#endif //WALLENGINE_CORE_H
