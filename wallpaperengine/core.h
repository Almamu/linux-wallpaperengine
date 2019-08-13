#pragma once

#include <string>

#include <irrlicht/irrlicht.h>

namespace wp::core
{
    irr::core::vector3df ato3vf (const char *str);
    irr::core::vector2df ato2vf (const char *str);

    irr::core::vector3df ato3vf (const std::string& str);
    irr::core::vector2df ato2vf (const std::string& str);
};
