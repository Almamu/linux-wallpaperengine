#pragma once

#include <vector>
#include <glm/vec4.hpp>

#include "WallpaperEngine/Input/CMouseInput.h"
#include "CWallpaper.h"
#include <X11/Xlib.h>

using namespace WallpaperEngine::Input;

namespace WallpaperEngine::Render
{
    class CWallpaper;

    class CContext
    {
    public:
        CContext (std::vector <std::string> screens, GLFWwindow* window);
        ~CContext ();

        void initializeViewports ();
        void render ();
        void setWallpaper (CWallpaper* wallpaper);
        void setDefaultViewport (glm::vec4 defaultViewport);
        CMouseInput* getMouse () const;
        void setMouse (CMouseInput* mouse);

    private:
        Display* m_display;
        Pixmap m_pixmap;
        GC m_gc;
        XImage* m_image;
        GLFWwindow* m_window;
        char* m_imageData;
        CFBO* m_fbo;
        std::vector <std::string> m_screens;
        std::vector <glm::ivec4> m_viewports;
        glm::vec4 m_defaultViewport;
        CWallpaper* m_wallpaper;
        CMouseInput* m_mouse;
        bool m_isRootWindow;
    };
}