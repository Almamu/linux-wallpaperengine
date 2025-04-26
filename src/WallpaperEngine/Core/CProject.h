#pragma once

#include <map>
#include <nlohmann/json.hpp>

#include "CWallpaper.h"
#include "WallpaperEngine/Core/Core.h"
#include "WallpaperEngine/Core/Projects/CProperty.h"

#include "WallpaperEngine/Assets/CContainer.h"

namespace WallpaperEngine::Core::Projects {
class CProperty;
}

namespace WallpaperEngine::Core {
using json = nlohmann::json;
using namespace WallpaperEngine::Assets;

class CWallpaper;

class CProject {
  public:
    CProject (
        std::string title, std::string type, std::string workshopid, std::shared_ptr<const CContainer> container,
        bool supportsaudioprocessing, const std::map<std::string, Projects::CProperty*>& properties);
    ~CProject();

    static std::unique_ptr<CProject> fromFile (const std::string& filename, std::shared_ptr<const CContainer> container);

    [[nodiscard]] const CWallpaper* getWallpaper () const;
    [[nodiscard]] const std::string& getTitle () const;
    [[nodiscard]] const std::string& getType () const;
    [[nodiscard]] const std::map<std::string, Projects::CProperty*>& getProperties () const;
    [[nodiscard]] const std::string& getWorkshopId () const;

    [[nodiscard]] bool supportsAudioProcessing () const;

    [[nodiscard]] std::shared_ptr<const CContainer> getContainer () const;

  protected:
    void setWallpaper (const CWallpaper* wallpaper);

  private:
    std::map<std::string, Projects::CProperty*> m_properties;

    const std::string m_workshopid;
    const std::string m_title;
    const std::string m_type;
    const bool m_supportsaudioprocessing;
    const CWallpaper* m_wallpaper;
    std::shared_ptr<const CContainer> m_container;
};
} // namespace WallpaperEngine::Core
