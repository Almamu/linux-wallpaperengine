#pragma once

#include <irrlicht/irrlicht.h>

#include "CWallpaper.h"
#include "WallpaperEngine/Core/Core.h"
#include "WallpaperEngine/Core/Projects/CProperty.h"

namespace WallpaperEngine::Core
{
    using json = nlohmann::json;

    class CWallpaper;

    class CProject
    {
    public:
        static CProject* fromFile (const irr::io::path& filename);

        CWallpaper* getWallpaper () const;

        const std::string& getTitle () const;
        const std::string& getType () const;
        const std::vector<Projects::CProperty*>& getProperties () const;

    protected:
        CProject (std::string title, std::string type, CWallpaper* wallpaper);

        void insertProperty (Projects::CProperty* property);
    private:
        std::vector<Projects::CProperty*> m_properties;

        std::string m_title;
        std::string m_type;
        CWallpaper* m_wallpaper;
    };
};
