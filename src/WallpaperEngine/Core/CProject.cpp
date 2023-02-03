#include "common.h"
#include <WallpaperEngine/Assets/CContainer.h>
#include <WallpaperEngine/FileSystem/FileSystem.h>

#include "CProject.h"
#include "CScene.h"
#include "CVideo.h"

using namespace WallpaperEngine::Core;
using namespace WallpaperEngine::Assets;

CProject::CProject (std::string title, std::string type, CWallpaper* wallpaper, CContainer* container) :
    m_title (std::move (title)),
    m_type (std::move (type)),
    m_wallpaper (wallpaper),
    m_container (container)
{
    this->m_wallpaper->setProject (this);
}

CProject* CProject::fromFile (const std::string& filename, CContainer* container)
{
    json content = json::parse (WallpaperEngine::FileSystem::loadFullFile (filename, container));

    std::string title = *jsonFindRequired (content, "title", "Project title missing");
    std::string type = *jsonFindRequired (content, "type", "Project type missing");
    std::string file = *jsonFindRequired (content, "file", "Project's main file missing");
    auto general = content.find ("general");
    CWallpaper* wallpaper;

    std::transform (type.begin (), type.end (), type.begin (), tolower);

    if (type == "scene")
    {
        wallpaper = CScene::fromFile (file, container);
    }
    else if (type == "video")
    {
        wallpaper = new CVideo (file.c_str ());
    }
    else if (type == "web")
        sLog.exception ("Web wallpapers are not supported yet");
    else
        sLog.exception ("Unsupported wallpaper type: ", type);

    CProject* project = new CProject (
        title,
        type,
        wallpaper,
        container
    );

    if (general != content.end ())
    {
        auto properties = (*general).find ("properties");

        if (properties != (*general).end ())
        {
            for (const auto& cur : (*properties).items ())
            {
                Projects::CProperty* property = Projects::CProperty::fromJSON (cur.value (), cur.key ());

                if (property != nullptr)
                    project->insertProperty (property);
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

CContainer* CProject::getContainer ()
{
    return this->m_container;
}

void CProject::insertProperty (Projects::CProperty* property)
{
    this->m_properties.push_back (property);
}