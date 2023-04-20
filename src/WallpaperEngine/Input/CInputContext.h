#pragma once

#include "WallpaperEngine/Render/Drivers/CX11OpenGLDriver.h"
#ifdef ENABLE_WAYLAND
#include "WallpaperEngine/Render/Drivers/CWaylandOpenGLDriver.h"
#endif
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
        #ifdef ENABLE_WAYLAND
        explicit CInputContext (Render::Drivers::CWaylandOpenGLDriver& videoDriver);
        #endif
        void update ();

        [[nodiscard]] const CMouseInput& getMouseInput () const;

    private:
        std::unique_ptr<CMouseInput> m_mouse;
    };
}
