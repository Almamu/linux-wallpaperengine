#pragma once

#include "WallpaperEngine/Render/Drivers/CX11OpenGLDriver.h"
#include "CMouseInput.h"

namespace WallpaperEngine::Render::Drivers
{
    class CX11OpenGLDriver;
}

namespace WallpaperEngine::Input
{
    class CInputContext
    {
    public:
        explicit CInputContext (Render::Drivers::CX11OpenGLDriver& videoDriver);
        void update ();

        [[nodiscard]] const CMouseInput& getMouseInput () const;

    private:
        CMouseInput m_mouse;
    };
}
