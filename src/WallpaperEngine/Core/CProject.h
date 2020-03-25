#pragma once

#include <irrlicht/irrlicht.h>

#include "CScene.h"
#include "WallpaperEngine/Core/Core.h"
#include "WallpaperEngine/Core/Projects/CProperty.h"

namespace WallpaperEngine::Core
{
    using json = nlohmann::json;

    class CScene;

    class CProject
    {
    public:
        static CProject* fromFile (const irr::io::path& filename);

        const CScene* getScene () const;

        const std::string& getTitle () const;
        const std::string& getType () const;
        const std::vector<Projects::CProperty*>& getProperties () const;

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

