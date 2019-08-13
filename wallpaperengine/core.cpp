#include <irrlicht/fast_atof.h>
#include "core.h"

irr::core::vector3df wp::core::ato3vf(const char *str)
{
    irr::f32 x = irr::core::fast_atof (str, &str); while (*str == ' ') str ++;
    irr::f32 y = irr::core::fast_atof (str, &str); while (*str == ' ') str ++;
    irr::f32 z = irr::core::fast_atof (str, &str);

    return irr::core::vector3df (x, y, z);
}

irr::core::vector2df wp::core::ato2vf (const char *str)
{
    irr::f32 x = irr::core::fast_atof (str, &str); while (*str == ' ') str ++;
    irr::f32 y = irr::core::fast_atof (str, &str);

    return irr::core::vector2df (x, y);
}

irr::core::vector3df wp::core::ato3vf (const std::string& str)
{
    return wp::core::ato3vf (str.c_str ());
}

irr::core::vector2df wp::core::ato2vf (const std::string& str)
{
    return wp::core::ato2vf (str.c_str ());
}