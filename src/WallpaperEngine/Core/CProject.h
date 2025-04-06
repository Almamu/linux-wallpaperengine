#pragma once

#include "CWallpaper.h"
#include "WallpaperEngine/Core/Core.h"
#include "WallpaperEngine/Core/Projects/CProperty.h"

#include "WallpaperEngine/Assets/CContainer.h"

namespace WallpaperEngine::Core {
using json = nlohmann::json;
using namespace WallpaperEngine::Assets;

class CWallpaper;

class CProject {
  public:
    static CProject* fromFile (const std::string& filename, CContainer* container);

    [[nodiscard]] CWallpaper* getWallpaper () const;

    [[nodiscard]] const std::string& getTitle () const;
    [[nodiscard]] const std::string& getType () const;
    [[nodiscard]] const std::vector<Projects::CProperty*>& getProperties () const;
    [[nodiscard]] const std::string& getWorkshopId () const;

    CContainer* getContainer ();

  protected:
    CProject (std::string title, std::string type, std::string  workshopid, CContainer* container);

    void setWallpaper (CWallpaper* wallpaper);
    void insertProperty (Projects::CProperty* property);

  private:
    std::vector<Projects::CProperty*> m_properties;

    std::string m_workshopid;
    std::string m_title;
    std::string m_type;
    CWallpaper* m_wallpaper;
    CContainer* m_container;
};
} // namespace WallpaperEngine::Core
