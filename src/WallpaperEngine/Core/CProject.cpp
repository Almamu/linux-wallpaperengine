#include <WallpaperEngine/Assets/CContainer.h>

#include <utility>

#include "CProject.h"
#include "WallpaperEngine/Core/Wallpapers/CScene.h"
#include "WallpaperEngine/Core/Wallpapers/CVideo.h"
#include "WallpaperEngine/Core/Wallpapers/CWeb.h"
#include "WallpaperEngine/Logging/CLog.h"

using namespace WallpaperEngine::Core;
using namespace WallpaperEngine::Core::Wallpapers;
using namespace WallpaperEngine::Assets;

static int backgroundId = -1;

CProject::CProject (std::string title, std::string type, std::string  workshopid, CContainer* container) :
    m_workshopid(std::move(workshopid)),
    m_title (std::move (title)),
    m_type (std::move (type)),
    m_wallpaper (nullptr),
    m_container (container)
{}

CProject* CProject::fromFile (const std::string& filename, CContainer* container) {
    json content = json::parse (container->readFileAsString (filename));

    std::string dependency = jsonFindDefault<std::string> (content, "dependency", "No dependency");
    if (dependency == "No dependency") {
        // workshopid is not required, but we have to use it for some identification stuff,
        // so using a static, decreasing number should be enough
        std::string workshopid = jsonFindDefault <std::string> (content, "workshopid", std::to_string (backgroundId--));
        std::string title = *jsonFindRequired (content, "title", "Project title missing");
        std::string type = *jsonFindRequired (content, "type", "Project type missing");
        std::string file = *jsonFindRequired (content, "file", "Project's main file missing");
        auto general = content.find ("general");
        CWallpaper* wallpaper;

        std::transform (type.begin (), type.end (), type.begin (), tolower);

        CProject* project = new CProject (title, type, workshopid, container);

        if (type == "scene")
            wallpaper = CScene::fromFile (file, *project, container);
        else if (type == "video")
            wallpaper = new CVideo (file.c_str (), *project);
        else if (type == "web")
            wallpaper = new CWeb (file.c_str (), *project);
        else
            sLog.exception ("Unsupported wallpaper type: ", type);

        project->setWallpaper (wallpaper);

        if (general != content.end ()) {
            const auto properties = general->find ("properties");

            if (properties != general->end ()) {
                for (const auto& cur : properties->items ()) {
                    Projects::CProperty* property = Projects::CProperty::fromJSON (cur.value (), cur.key ());
                    if (property != nullptr)
                        project->insertProperty (property);
                }
            }
        }
        return project;
    } else {
        sLog.exception ("Project have dependency. They are not supported, quiting");
    }
}

void CProject::setWallpaper (CWallpaper* wallpaper) {
    this->m_wallpaper = wallpaper;
}

CWallpaper* CProject::getWallpaper () const {
    return this->m_wallpaper;
}

const std::string& CProject::getTitle () const {
    return this->m_title;
}

const std::string& CProject::getType () const {
    return this->m_type;
}

const std::vector<Projects::CProperty*>& CProject::getProperties () const {
    return this->m_properties;
}

const std::string& CProject::getWorkshopId () const {
    return this->m_workshopid;
}

CContainer* CProject::getContainer () {
    return this->m_container;
}

void CProject::insertProperty (Projects::CProperty* property) {
    this->m_properties.push_back (property);
}