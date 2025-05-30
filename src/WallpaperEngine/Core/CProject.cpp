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

CProject::CProject (
    std::string title, std::string type, std::string workshopid, std::shared_ptr<const CContainer> container,
    bool supportsaudioprocessing, const std::map<std::string, std::shared_ptr<Projects::CProperty>>& properties
) :
    m_workshopid(std::move(workshopid)),
    m_title (std::move(title)),
    m_type (std::move(type)),
    m_container (std::move(container)),
    m_properties (properties),
    m_supportsaudioprocessing (supportsaudioprocessing) {}

std::shared_ptr<CProject> CProject::fromFile (const std::string& filename, std::shared_ptr<const CContainer> container) {
    json content = json::parse (container->readFileAsString (filename));

    const auto dependency = jsonFindDefault<std::string> (content, "dependency", "No dependency");

    if (dependency != "No dependency") {
        sLog.exception ("Project have dependency. They are not supported, quiting");
    }

    // workshopid is not required, but we have to use it for some identification stuff,
    // so using a static, decreasing number should be enough
    bool supportsaudioprocessing = false;
    auto type = jsonFindRequired <std::string> (content, "type", "Project type missing");
    const auto file = jsonFindRequired <std::string> (content, "file", "Project's main file missing");
    auto general = content.find ("general");
    std::shared_ptr <const CWallpaper> wallpaper = nullptr;
    std::map<std::string, std::shared_ptr <Projects::CProperty>> properties;

    std::transform (type.begin (), type.end (), type.begin (), tolower);

    if (general != content.end ()) {
        supportsaudioprocessing = jsonFindDefault (general, "supportsaudioprocessing", false);
        const auto properties_it = general->find ("properties");

        if (properties_it != general->end ()) {
            for (const auto& cur : properties_it->items ()) {
                auto property = Projects::CProperty::fromJSON (cur.value (), cur.key ());

                if (property == nullptr) {
                    continue;
                }

                properties.emplace (property->getName (), std::move (property));
            }
        }
    }

    std::shared_ptr<CProject> project = std::make_shared <CProject> (
        jsonFindRequired <std::string> (content, "title", "Project title missing"),
        type,
        jsonFindDefault <std::string> (content, "workshopid", std::to_string (backgroundId--)),
        container,
        supportsaudioprocessing,
        properties
    );

    if (type == "scene")
        wallpaper = CScene::fromFile (file, project, container);
    else if (type == "video")
        wallpaper = std::make_shared<CVideo> (file, project);
    else if (type == "web")
        wallpaper = std::make_shared<CWeb> (file, project);
    else
        sLog.exception ("Unsupported wallpaper type: ", type);

    project->setWallpaper (wallpaper);

    return project;
}

void CProject::setWallpaper (std::shared_ptr <const CWallpaper> wallpaper) {
    this->m_wallpaper = wallpaper;
}

const std::shared_ptr <const CWallpaper> CProject::getWallpaper () const {
    return this->m_wallpaper;
}

const std::string& CProject::getTitle () const {
    return this->m_title;
}

const std::string& CProject::getType () const {
    return this->m_type;
}

const std::map<std::string, std::shared_ptr <Projects::CProperty>>& CProject::getProperties () const {
    return this->m_properties;
}

const std::string& CProject::getWorkshopId () const {
    return this->m_workshopid;
}

std::shared_ptr<const CContainer> CProject::getContainer () const {
    return this->m_container;
}

bool CProject::supportsAudioProcessing () const {
    return this->m_supportsaudioprocessing;
}