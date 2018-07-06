#ifndef WALLENGINE_MATERIAL_H
#define WALLENGINE_MATERIAL_H

#include <nlohmann/json.hpp>

namespace wp
{
    using json = nlohmann::json;

    class material
    {
        material(json json_data);
    };
}


#endif //WALLENGINE_MATERIAL_H
