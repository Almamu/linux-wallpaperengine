#pragma once

#include <condition_variable>
#include <mutex>
#include "CAudioPlayingDetector.h"
#include <pulse/pulseaudio.h>

namespace WallpaperEngine::Audio::Drivers::Detectors
{
    class CPulseAudioPlayingDetector : public CAudioPlayingDetector
    {
    public:
        explicit CPulseAudioPlayingDetector (Application::CApplicationContext& appContext, Render::Drivers::Detectors::CFullScreenDetector&);
        ~CPulseAudioPlayingDetector ();

        void update () override;

    private:
        pa_mainloop* m_mainloop;
        pa_mainloop_api* m_mainloopApi;
        pa_context* m_context;
    };
}
