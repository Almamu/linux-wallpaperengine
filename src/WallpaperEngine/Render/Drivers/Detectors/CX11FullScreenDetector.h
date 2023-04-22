#pragma once

#include <string>
#include <vector>
#include <glm/vec4.hpp>

#include "CFullScreenDetector.h"
#include <X11/Xlib.h>

namespace WallpaperEngine::Render::Drivers
{
    class CX11OpenGLDriver;

    namespace Detectors
    {
        class CX11FullScreenDetector : public CFullScreenDetector
        {
        public:
            CX11FullScreenDetector (Application::CApplicationContext& appContext, CX11OpenGLDriver& driver);
            ~CX11FullScreenDetector ();

            [[nodiscard]] bool anythingFullscreen () const override;
            void reset () override;

        private:
            void initialize ();
            void stop ();

            struct ScreenInfo
            {
                glm::ivec4 viewport;
                std::string name;
            };

            Display* m_display;
            Window m_root;
            std::vector <ScreenInfo> m_screens;
            CX11OpenGLDriver& m_driver;
        };
    }
}