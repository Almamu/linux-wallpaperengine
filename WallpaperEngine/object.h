#ifndef WALLENGINE_OBJECT_H
#define WALLENGINE_OBJECT_H

#include <iostream>
#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>
#include <WallpaperEngine/video/node.h>
#include <WallpaperEngine/effect.h>
#include <WallpaperEngine/scene.h>

namespace WallpaperEngine
{
    using json = nlohmann::json;

    class object3d;
    class scene;
    class effect;

    class object : public WallpaperEngine::video::node
    {
    public:

        object (json json_data, WallpaperEngine::scene* scene);
        ~object ();

        irr::core::vector2df& getSize ();
        irr::core::vector3df& getScale ();
        irr::core::vector3df& getOrigin ();

        irr::core::vector3df& getAngles ();

        std::vector<effect*>& getEffects ();

        WallpaperEngine::scene* getScene ();

        void render ();
    private:
        int m_id;

        WallpaperEngine::scene* m_scene;

        std::string m_name;

        irr::core::vector2df m_size;
        irr::core::vector3df m_scale;
        irr::core::vector3df m_origin;

        irr::core::vector3df m_angles;
        WallpaperEngine::object3d* m_object3d;
        std::vector<effect*> m_effects;
    };
};

#endif //WALLENGINE_OBJECT_H
