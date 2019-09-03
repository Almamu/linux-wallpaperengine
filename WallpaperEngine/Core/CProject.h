#pragma once

#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>

#include "CScene.h"
#include "Projects/CProperty.h"

namespace WallpaperEngine::Core
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
        std::vector<Projects::CProperty*>* getProperties ();

    protected:
        CProject (std::string title, std::string type, CScene* scene);

        void insertProperty (Projects::CProperty* property);
    private:
        std::vector<Projects::CProperty*> m_properties;

        std::string m_title;
        std::string m_type;
        CScene* m_scene;
    };
};

