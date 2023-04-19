#pragma once

#include <string>
#include <vector>
#include <glm/vec4.hpp>

#include "CFullScreenDetector.h"
#include "WallpaperEngine/Render/Drivers/CVideoDriver.h"
#include "WallpaperEngine/Render/Drivers/CWaylandOpenGLDriver.h"

namespace WallpaperEngine::Render::Drivers::Detectors
{
    class CWaylandFullScreenDetector : public CFullScreenDetector
    {
    public:
        CWaylandFullScreenDetector (Application::CApplicationContext& appContext, CWaylandOpenGLDriver& driver);
        ~CWaylandFullScreenDetector ();

        [[nodiscard]] bool anythingFullscreen () const override;
        void reset () override;

    private:
    };
}