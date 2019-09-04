#ifndef WALLENGINE_PROJECT_H
#define WALLENGINE_PROJECT_H

#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>
#include "scene.h"

namespace WallpaperEngine
{
    using json = nlohmann::json;

    class project
    {
    public:
        project (irr::io::path& jsonfile_path);

        scene* getScene ();

    private:
        json m_projectFile;
        std::string m_content;

        std::string m_title;
        std::string m_type;
        irr::io::path m_file;
        scene* m_scene;
    };
};

#endif //WALLENGINE_PROJECT_H
