#ifndef WALLENGINE_OBJECT_H
#define WALLENGINE_OBJECT_H

#include <iostream>
#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>
#include "effect.h"

namespace wp
{
    using json = nlohmann::json;

    class object3d;
    class object
    {
    public:

        object (json json_data, irr::io::path basepath);
        ~object ();

    private:
        int m_id;

        std::string m_name;

        irr::core::vector2df m_size;
        irr::core::vector3df m_scale;
        irr::core::vector3df m_origin;

        irr::core::vector3df m_angles;
        wp::object3d* m_object3d;
        std::vector<effect*> m_effects;
    };
};

#endif //WALLENGINE_OBJECT_H
