#pragma once

#include <vector>
#include <glm/vec4.hpp>

#include "WallpaperEngine/Input/CMouseInput.h"
#include "WallpaperEngine/Render/Drivers/CVideoDriver.h"
#include "CTextureCache.h"
#include "CWallpaper.h"
#include <X11/Xlib.h>

using namespace WallpaperEngine::Assets;
using namespace WallpaperEngine::Input;
using namespace WallpaperEngine::Render::Drivers;

namespace WallpaperEngine::Render::Drivers
{
    class CVideoDriver;
}

namespace WallpaperEngine::Render
{
    class CWallpaper;
    class CTextureCache;

    class CRenderContext
    {
    public:
        CRenderContext (std::vector <std::string> screens, CVideoDriver& driver, CContainer* container);
        ~CRenderContext ();

        void initialize ();
        void render ();
        void setWallpaper (CWallpaper* wallpaper);
        CMouseInput* getMouse () const;
        void setMouse (CMouseInput* mouse);
        CWallpaper* getWallpaper () const;
        const CContainer* getContainer () const;
        const ITexture* resolveTexture (const std::string& name);

    private:
        void setupScreens ();
        void setupWindow ();

        void renderScreens ();
        void renderWindow ();

        struct viewport
        {
            glm::ivec4 viewport;
            std::string name;
        };

        Display* m_display;
        Pixmap m_pixmap;
        GC m_gc;
        XImage* m_image;
        CVideoDriver& m_driver;
        char* m_imageData;
        CFBO* m_fbo;
        std::vector <std::string> m_screens;
        std::vector <viewport> m_viewports;
        CWallpaper* m_wallpaper;
        CMouseInput* m_mouse;
        CContainer* m_container;
        CTextureCache* m_textureCache;
    };
}