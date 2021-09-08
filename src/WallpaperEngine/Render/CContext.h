#pragma once

#include <vector>
#include <glm/vec4.hpp>

#include "CWallpaper.h"

namespace WallpaperEngine::Render
{
    class CContext
    {
    public:
        CContext (std::vector <std::string> screens);

        void initializeViewports ();
        void render ();
        void setWallpaper (CWallpaper* wallpaper);
        void setDefaultViewport (glm::vec4 defaultViewport);

    private:
        Window m_window;
        std::vector <std::string> m_screens;
        std::vector <glm::vec4> m_viewports;
        glm::vec4 m_defaultViewport;
        CWallpaper* m_wallpaper;
        bool m_isRootWindow;
    };
}