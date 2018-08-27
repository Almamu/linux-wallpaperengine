#ifndef WALLENGINE_OBJECT_H
#define WALLENGINE_OBJECT_H

#include <iostream>
#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>
#include <wallpaperengine/video/node.h>
#include <wallpaperengine/effect.h>
#include <wallpaperengine/scene.h>

namespace wp
{
    using json = nlohmann::json;

    class object3d;
    class scene;

    class object : public wp::video::node
    {
    public:

        object (json json_data, wp::scene* scene);
        ~object ();

        void render ();
    private:
        int m_id;

        wp::scene* m_scene;

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
