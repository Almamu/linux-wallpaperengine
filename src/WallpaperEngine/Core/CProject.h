#pragma once

#include "CWallpaper.h"
#include "WallpaperEngine/Core/Core.h"
#include "WallpaperEngine/Core/Projects/CProperty.h"

#include "WallpaperEngine/Assets/CContainer.h"

namespace WallpaperEngine::Core
{
    using json = nlohmann::json;
    using namespace WallpaperEngine::Assets;

    class CWallpaper;

    class CProject
    {
    public:
        static CProject* fromFile (const std::string& filename, CContainer* container);

        CWallpaper* getWallpaper () const;

        const std::string& getTitle () const;
        const std::string& getType () const;
        const std::vector<Projects::CProperty*>& getProperties () const;

        CContainer* getContainer ();

    protected:
        CProject (std::string title, std::string type, CWallpaper* wallpaper, CContainer* container);

        void insertProperty (Projects::CProperty* property);
    private:
        std::vector<Projects::CProperty*> m_properties;

        std::string m_title;
        std::string m_type;
        CWallpaper* m_wallpaper;
        CContainer* m_container;
    };
};
