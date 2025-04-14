#include <WallpaperEngine/Assets/CContainer.h>

#include "CProject.h"
#include "WallpaperEngine/Core/Wallpapers/CScene.h"
#include "WallpaperEngine/Core/Wallpapers/CVideo.h"
#include "WallpaperEngine/Core/Wallpapers/CWeb.h"
#include "WallpaperEngine/Logging/CLog.h"

using namespace WallpaperEngine::Core;
using namespace WallpaperEngine::Core::Wallpapers;
using namespace WallpaperEngine::Assets;

static int backgroundId = -1;

CProject::CProject (std::string title, std::string type, std::string workshopid, const CContainer* container) :
    m_workshopid(workshopid),
    m_title (title),
    m_type (type),
    m_wallpaper (nullptr),
    m_container (container) {}

CProject* CProject::fromFile (std::string filename, const CContainer* container) {
    json content = json::parse (container->readFileAsString (filename));

    const auto dependency = jsonFindDefault<std::string> (content, "dependency", "No dependency");

    if (dependency != "No dependency") {
        sLog.exception ("Project have dependency. They are not supported, quiting");
    }

    // workshopid is not required, but we have to use it for some identification stuff,
    // so using a static, decreasing number should be enough
    auto type = jsonFindRequired <std::string> (content, "type", "Project type missing");
    const auto file = jsonFindRequired <std::string> (content, "file", "Project's main file missing");
    auto general = content.find ("general");
    const CWallpaper* wallpaper;

    std::transform (type.begin (), type.end (), type.begin (), tolower);

    CProject* project = new CProject (
        jsonFindRequired <std::string> (content, "title", "Project title missing"),
        type,
        jsonFindDefault <std::string> (content, "workshopid", std::to_string (backgroundId--)),
        container
    );

    if (type == "scene")
        wallpaper = CScene::fromFile (file, *project, container);
    else if (type == "video")
        wallpaper = new CVideo (file, *project);
    else if (type == "web")
        wallpaper = new CWeb (file, *project);
    else
        sLog.exception ("Unsupported wallpaper type: ", type);

    project->setWallpaper (wallpaper);

    if (general != content.end ()) {
        const auto properties = general->find ("properties");

        if (properties != general->end ()) {
            for (const auto& cur : properties->items ()) {
                const auto property = Projects::CProperty::fromJSON (cur.value (), cur.key ());
                if (property != nullptr)
                    project->insertProperty (property);
            }
        }
    }
    return project;
}

void CProject::setWallpaper (const CWallpaper* wallpaper) {
    this->m_wallpaper = wallpaper;
}

const CWallpaper* CProject::getWallpaper () const {
    return this->m_wallpaper;
}

const std::string& CProject::getTitle () const {
    return this->m_title;
}

const std::string& CProject::getType () const {
    return this->m_type;
}

const std::vector<const Projects::CProperty*>& CProject::getProperties () const {
    return this->m_properties;
}

const std::string& CProject::getWorkshopId () const {
    return this->m_workshopid;
}

const CContainer* CProject::getContainer () const {
    return this->m_container;
}

void CProject::insertProperty (const Projects::CProperty* property) {
    this->m_properties.push_back (property);
}