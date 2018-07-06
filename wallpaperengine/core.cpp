#include <irrlicht/fast_atof.h>
#include "core.h"

namespace wp
{
    irr::core::vector3df core::ato3vf(const char *str)
    {
        irr::f32 x = irr::core::fast_atof (str, &str); while (*str == ' ') str ++;
        irr::f32 y = irr::core::fast_atof (str, &str); while (*str == ' ') str ++;
        irr::f32 z = irr::core::fast_atof (str, &str);

        return irr::core::vector3df (x, y, z);
    }

    irr::core::vector2df core::ato2vf (const char *str)
    {
        irr::f32 x = irr::core::fast_atof (str, &str); while (*str == ' ') str ++;
        irr::f32 y = irr::core::fast_atof (str, &str);

        return irr::core::vector2df (x, y);
    }
}