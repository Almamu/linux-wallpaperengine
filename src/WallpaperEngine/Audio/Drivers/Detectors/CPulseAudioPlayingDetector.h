#pragma once

#include "CAudioPlayingDetector.h"
#include <condition_variable>
#include <mutex>
#include <pulse/pulseaudio.h>

namespace WallpaperEngine::Audio::Drivers::Detectors {
class CPulseAudioPlayingDetector final : public CAudioPlayingDetector {
  public:
    explicit CPulseAudioPlayingDetector (
        Application::CApplicationContext& appContext, const Render::Drivers::Detectors::CFullScreenDetector*);
    ~CPulseAudioPlayingDetector () override;

    void update () override;

  private:
    pa_mainloop* m_mainloop;
    pa_mainloop_api* m_mainloopApi;
    pa_context* m_context;
};
} // namespace WallpaperEngine::Audio::Drivers::Detectors
