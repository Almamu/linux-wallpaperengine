#ifndef WALLENGINE_SCENE_H
#define WALLENGINE_SCENE_H

#include <iostream>
#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>
#include "Object.h"

using json = nlohmann::json;

class Scene
{
public:
    Scene(irr::io::path file);

private:
    irr::io::path m_file;
    std::string m_content;
    std::vector<Object*> m_objects;
    json m_json;
};


#endif //WALLENGINE_SCENE_H
