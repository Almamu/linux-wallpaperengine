#pragma once

#include <vector>
#include <glm/vec4.hpp>

#include <X11/Xlib.h>

#include "WallpaperEngine/Input/CMouseInput.h"
#include "CWallpaper.h"

using namespace WallpaperEngine::Input;

namespace WallpaperEngine::Render
{
    class CWallpaper;

    class CContext
    {
    public:
        CContext (std::vector <std::string> screens);

        void initializeViewports ();
        void render ();
        void setWallpaper (CWallpaper* wallpaper);
        void setDefaultViewport (glm::vec4 defaultViewport);
        CMouseInput* getMouse () const;
        void setMouse (CMouseInput* mouse);
    private:
        std::vector <std::string> m_screens;
        std::vector <glm::vec4> m_viewports;
        glm::vec4 m_defaultViewport;
        CWallpaper* m_wallpaper;
        CMouseInput* m_mouse;
        bool m_isRootWindow;
        Display* m_display;
        Pixmap m_pm;
        GC m_gc;
    };
}