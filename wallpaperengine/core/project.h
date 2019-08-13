#pragma once

#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>

#include "scene.h"

namespace wp::core
{
    using json = nlohmann::json;

    class scene;

    class project
    {
    public:
        static project* fromFile (const irr::io::path& filename);

        scene* getScene ();

        std::string getTitle ();
        std::string getType ();
    protected:
        project (std::string title, std::string type, scene* scene);
    private:
        std::string m_title;
        std::string m_type;
        scene* m_scene;
    };
};

