#ifndef WALLENGINE_SCENE_H
#define WALLENGINE_SCENE_H

#include <iostream>
#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>
#include "object.h"
#include "camera.h"

namespace wp
{
    using json = nlohmann::json;

    class scene
    {
    public:
        scene (irr::io::path file, irr::io::path basepath);
        ~scene ();

    private:
        irr::io::path m_file;
        std::string m_content;
        camera* m_camera;
        std::vector<object*> m_objects;
        json m_json;
    };
};

#endif //WALLENGINE_SCENE_H
