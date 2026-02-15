#pragma once

#include <glm/vec4.hpp>
#include <memory>
#include <vector>

#include "TextureCache.h"
#include "WallpaperEngine/Application/WallpaperApplication.h"
#include "WallpaperEngine/Input/InputContext.h"
#include "WallpaperEngine/Input/MouseInput.h"
#include "WallpaperEngine/Render/Drivers/Output/Output.h"
#include "WallpaperEngine/Render/Drivers/Output/OutputViewport.h"
#include "WallpaperEngine/Render/Drivers/VideoDriver.h"

namespace WallpaperEngine {
namespace Application {
    class WallpaperApplication;
}

namespace Render {
    namespace Drivers {
	class VideoDriver;

	namespace Output {
	    class Output;
	    class OutputViewport;
	} // namespace Output
    } // namespace Drivers

    class CWallpaper;
    class TextureCache;

    class RenderContext {
    public:
	RenderContext (Drivers::VideoDriver& driver, WallpaperApplication& app);

        void renderTextures ();
	void render (Drivers::Output::OutputViewport* viewport);
	void setWallpaper (const std::string& display, std::shared_ptr<CWallpaper> wallpaper);
	void setPause (bool newState) const;
	[[nodiscard]] Input::InputContext& getInputContext () const;
	[[nodiscard]] const WallpaperApplication& getApp () const;
	[[nodiscard]] const Drivers::VideoDriver& getDriver () const;
	[[nodiscard]] const Drivers::Output::Output& getOutput () const;
	[[nodiscard]] std::shared_ptr<const TextureProvider> resolveTexture (const std::string& name) const;
	[[nodiscard]] const std::map<std::string, std::shared_ptr<CWallpaper>>& getWallpapers () const;

    private:
	/** Video driver in use */
	Drivers::VideoDriver& m_driver;
	/** Maps screen -> wallpaper list */
	std::map<std::string, std::shared_ptr<CWallpaper>> m_wallpapers = {};
	/** App that holds the render context */
	WallpaperApplication& m_app;
	/** Texture cache for the render */
	TextureCache* m_textureCache = nullptr;
    };
} // namespace Render
} // namespace WallpaperEngine
