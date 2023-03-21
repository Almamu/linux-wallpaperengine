#pragma once

#include <vector>
#include <glm/vec4.hpp>

#include "CTextureCache.h"
#include "CWallpaper.h"
#include "WallpaperEngine/Application/CWallpaperApplication.h"
#include "WallpaperEngine/Input/CInputContext.h"
#include "WallpaperEngine/Input/CMouseInput.h"
#include "WallpaperEngine/Render/Drivers/CVideoDriver.h"
#include "WallpaperEngine/Render/Drivers/Output/COutput.h"
#include <X11/Xlib.h>

using namespace WallpaperEngine::Application;
using namespace WallpaperEngine::Assets;
using namespace WallpaperEngine::Input;
using namespace WallpaperEngine::Render::Drivers;
using namespace WallpaperEngine::Render::Drivers::Output;

namespace WallpaperEngine::Render::Drivers::Output
{
	class COutput;
}

namespace WallpaperEngine::Render::Drivers
{
    class CVideoDriver;
}

namespace WallpaperEngine::Application
{
    class CWallpaperApplication;
}

namespace WallpaperEngine::Render
{
    class CWallpaper;
    class CTextureCache;

    class CRenderContext
    {
    public:
        CRenderContext (const COutput* output, CVideoDriver& driver, CInputContext& input, CWallpaperApplication& app);

        void render ();
		void setDefaultWallpaper (CWallpaper* wallpaper);
		void setWallpaper (const std::string& display, CWallpaper* wallpaper);
        CInputContext& getInputContext () const;
        const CWallpaperApplication& getApp () const;
		const COutput* getOutput () const;
        const ITexture* resolveTexture (const std::string& name);

    private:
        CVideoDriver& m_driver;
		std::map <std::string, CWallpaper*> m_wallpapers;
        CWallpaper* m_defaultWallpaper;
        CInputContext& m_input;
        CWallpaperApplication& m_app;
        CTextureCache* m_textureCache;
		const COutput* m_output;
    };
}