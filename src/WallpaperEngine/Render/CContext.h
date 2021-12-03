#pragma once

#include <vector>
#include <glm/vec4.hpp>

#include "WallpaperEngine/Input/CMouseInput.h"
#include "CWallpaper.h"

using namespace WallpaperEngine::Input;

namespace WallpaperEngine::Render
{
    class CWallpaper;

    class CContext
    {
    public:
        CContext (std::vector <std::string> screens, CMouseInput* mouse);

        void initializeViewports ();
        void render ();
        void setWallpaper (CWallpaper* wallpaper);
        void setDefaultViewport (glm::vec4 defaultViewport);
        CMouseInput* getMouse () const;
    private:
        std::vector <std::string> m_screens;
        std::vector <glm::vec4> m_viewports;
        glm::vec4 m_defaultViewport;
        CWallpaper* m_wallpaper;
        CMouseInput* m_mouse;
        bool m_isRootWindow;
    };
}