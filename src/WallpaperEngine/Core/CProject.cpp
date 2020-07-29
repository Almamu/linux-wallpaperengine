#include <WallpaperEngine/FileSystem/FileSystem.h>

#include "CProject.h"
#include "CScene.h"
#include "CVideo.h"

using namespace WallpaperEngine::Core;

CProject::CProject (std::string title, std::string type, CWallpaper* wallpaper) :
    m_title (std::move (title)),
    m_type (std::move (type)),
    m_wallpaper (wallpaper)
{
    this->m_wallpaper->setProject (this);
}

CProject* CProject::fromFile (const irr::io::path& filename)
{
    json content = json::parse (WallpaperEngine::FileSystem::loadFullFile (filename));

    std::string title = *jsonFindRequired (content, "title", "Project title missing");
    std::string type = *jsonFindRequired (content, "type", "Project type missing");
    std::string file = *jsonFindRequired (content, "file", "Project's main file missing");
    auto general = content.find ("general");
    CWallpaper* wallpaper;

    std::transform (type.begin (), type.end (), type.begin (), tolower);

    if (type == "scene")
    {
        wallpaper = CScene::fromFile (file.c_str ());
    }
    else if (type == "video")
    {
        wallpaper = new CVideo (file.c_str ());
    }
    else
        throw std::runtime_error ("Unsupported wallpaper type");

    CProject* project = new CProject (
        title,
        type,
        wallpaper
    );

    if (general != content.end ())
    {
        auto properties = (*general).find ("properties");

        if (properties != (*general).end ())
        {
            auto cur = (*properties).begin ();
            auto end = (*properties).end ();

            for (; cur != end; cur ++)
            {
                project->insertProperty (
                        Projects::CProperty::fromJSON (*cur, cur.key ())
                );
            }
        }
    }

    return project;
}

CWallpaper* CProject::getWallpaper () const
{
    return this->m_wallpaper;
}

const std::string& CProject::getTitle () const
{
    return this->m_title;
}

const std::string& CProject::getType () const
{
    return this->m_type;
}

const std::vector<Projects::CProperty*>& CProject::getProperties () const
{
    return this->m_properties;
}

void CProject::insertProperty (Projects::CProperty* property)
{
    this->m_properties.push_back (property);
}