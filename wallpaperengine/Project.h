#ifndef WALLENGINE_PROJECT_H
#define WALLENGINE_PROJECT_H

#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>
#include "Scene.h"

using json = nlohmann::json;

class Project
{
public:
    Project (irr::io::path baseDirectory);

private:
    json m_projectFile;
    std::string m_content;

    std::string m_title;
    std::string m_type;
    std::string m_file;
    Scene* m_scene;
};


#endif //WALLENGINE_PROJECT_H
