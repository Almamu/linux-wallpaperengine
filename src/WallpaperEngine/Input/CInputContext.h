#pragma once

#include "WallpaperEngine/Render/Drivers/COpenGLDriver.h"
#include "CMouseInput.h"

namespace WallpaperEngine::Render::Drivers
{
    class COpenGLDriver;
}

namespace WallpaperEngine::Input
{
    class CInputContext
    {
    public:
        explicit CInputContext (Render::Drivers::COpenGLDriver& videoDriver);
        void update ();

        const CMouseInput& getMouseInput () const;

    private:
        CMouseInput m_mouse;
    };
}
