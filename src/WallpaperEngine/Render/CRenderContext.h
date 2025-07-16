#pragma once

#include <glm/vec4.hpp>
#include <vector>
#include <memory>

#include "CTextureCache.h"
#include "WallpaperEngine/Application/CWallpaperApplication.h"
#include "WallpaperEngine/Input/CInputContext.h"
#include "WallpaperEngine/Input/CMouseInput.h"
#include "WallpaperEngine/Render/Drivers/CVideoDriver.h"
#include "WallpaperEngine/Render/Drivers/Output/COutput.h"
#include "WallpaperEngine/Render/Drivers/Output/COutputViewport.h"

namespace WallpaperEngine {
namespace Application {
class CWallpaperApplication;
}

namespace Render {
namespace Drivers {
class CVideoDriver;

namespace Output {
class COutput;
class COutputViewport;
} // namespace Output
} // namespace Drivers

class CWallpaper;
class CTextureCache;

class CRenderContext {
  public:
    CRenderContext (Drivers::CVideoDriver& driver, CWallpaperApplication& app);

    void render (Drivers::Output::COutputViewport* viewport);
    void setWallpaper (const std::string& display, std::shared_ptr <CWallpaper> wallpaper);
    void setPause (bool newState);
    [[nodiscard]] Input::CInputContext& getInputContext () const;
    [[nodiscard]] const CWallpaperApplication& getApp () const;
    [[nodiscard]] const Drivers::CVideoDriver& getDriver () const;
    [[nodiscard]] const Drivers::Output::COutput& getOutput () const;
    std::shared_ptr<const ITexture> resolveTexture (const std::string& name);
    [[nodiscard]] const std::map<std::string, std::shared_ptr <CWallpaper>>& getWallpapers () const;

  private:
    /** Video driver in use */
    Drivers::CVideoDriver& m_driver;
    /** Maps screen -> wallpaper list */
    std::map<std::string, std::shared_ptr <CWallpaper>> m_wallpapers = {};
    /** App that holds the render context */
    CWallpaperApplication& m_app;
    /** Texture cache for the render */
    CTextureCache* m_textureCache = nullptr;
};
} // namespace Render
} // namespace WallpaperEngine
