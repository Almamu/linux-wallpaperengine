#pragma once

#include "WallpaperEngine/Application/CApplicationContext.h"

namespace WallpaperEngine::Application
{
    class CApplicationContext;
}

namespace WallpaperEngine::Audio::Detectors
{
    class CAudioPlayingDetector
    {
    public:
        CAudioPlayingDetector (Application::CApplicationContext& appContext);

        /**
         * @return If any kind of sound is currently playing on the default audio device
         */
        virtual bool anythingPlaying () = 0;
        /**
         * @return The application context using this detector
         */
        [[nodiscard]] Application::CApplicationContext& getApplicationContext ();

    private:
        Application::CApplicationContext& m_applicationContext;
    };
}
