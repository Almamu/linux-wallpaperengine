#pragma once

#include "WallpaperEngine/Render/Drivers/Detectors/CFullScreenDetector.h"
#include "WallpaperEngine/Application/CApplicationContext.h"

namespace WallpaperEngine
{
    namespace Application
    {
        class CApplicationContext;
    }

    namespace Render::Drivers::Detectors
    {
        class CFullScreenDetector;
    }

    namespace Audio::Detectors
    {
        class CAudioPlayingDetector
        {
        public:
            CAudioPlayingDetector (Application::CApplicationContext& appContext, Render::Drivers::Detectors::CFullScreenDetector& fullscreenDetector);

            /**
             * @return If any kind of sound is currently playing on the default audio device
             */
            virtual bool anythingPlaying () = 0;
            /**
             * @return The application context using this detector
             */
            [[nodiscard]] Application::CApplicationContext& getApplicationContext ();
            /**
             * @return The fullscreen detector used
             */
            [[nodiscard]] Render::Drivers::Detectors::CFullScreenDetector& getFullscreenDetector ();

        private:
            Application::CApplicationContext& m_applicationContext;
            Render::Drivers::Detectors::CFullScreenDetector& m_fullscreenDetector;
        };
    }
}
