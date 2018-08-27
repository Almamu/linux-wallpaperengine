#ifndef WALLENGINE_SCENE_H
#define WALLENGINE_SCENE_H

#include <iostream>
#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>
#include <wallpaperengine/object.h>
#include <wallpaperengine/camera.h>
#include <wallpaperengine/video/node.h>

namespace wp
{
    using json = nlohmann::json;

    class object;
    class scene : public wp::video::node
    {
    public:
        scene (irr::io::path file);
        ~scene ();

        camera* getCamera ();
        bool isOrthogonal ();
        float getProjectionWidth ();
        float getProjectionHeight ();
        void render ();

    private:
        float m_width;
        float m_height;
        bool m_isOrthogonal;

        irr::io::path m_file;
        std::string m_content;
        camera* m_camera;
        std::vector<object*> m_objects;
        json m_json;
    };
};

#endif //WALLENGINE_SCENE_H
