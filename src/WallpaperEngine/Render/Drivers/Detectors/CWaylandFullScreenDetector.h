#ifdef ENABLE_WAYLAND
#pragma once

#include <string>
#include <vector>
#include <glm/vec4.hpp>

#include "CFullScreenDetector.h"
#include "WallpaperEngine/Render/Drivers/CVideoDriver.h"

namespace WallpaperEngine::Render::Drivers
{
    class CWaylandOpenGLDriver;

    namespace Detectors
    {
        class CWaylandFullScreenDetector : public CFullScreenDetector
        {
        public:
            CWaylandFullScreenDetector (Application::CApplicationContext& appContext, CWaylandOpenGLDriver& driver);
            ~CWaylandFullScreenDetector ();

            [[nodiscard]] bool anythingFullscreen () const override;
            void reset () override;
        };
    }
}
#endif /* ENABLE_WAYLAND */