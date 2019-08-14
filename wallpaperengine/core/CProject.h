#pragma once

#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>

#include "CScene.h"

namespace wp::core
{
    using json = nlohmann::json;

    class CScene;

    class CProject
    {
    public:
        static CProject* fromFile (const irr::io::path& filename);

        CScene* getScene ();

        std::string getTitle ();
        std::string getType ();
    protected:
        CProject (std::string title, std::string type, CScene* scene);
    private:
        std::string m_title;
        std::string m_type;
        CScene* m_scene;
    };
};

