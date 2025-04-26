#pragma once

#include "CAudioPlayingDetector.h"
#include <condition_variable>
#include <mutex>
#include <pulse/pulseaudio.h>

namespace WallpaperEngine::Audio::Drivers::Detectors {
class CPulseAudioPlayingDetector final : public CAudioPlayingDetector {
  public:
    explicit CPulseAudioPlayingDetector (
        Application::CApplicationContext& appContext, const Render::Drivers::Detectors::CFullScreenDetector&);
    ~CPulseAudioPlayingDetector () override;

    void update () override;

  private:
    pa_mainloop* m_mainloop = nullptr;
    pa_mainloop_api* m_mainloopApi = nullptr;
    pa_context* m_context = nullptr;
};
} // namespace WallpaperEngine::Audio::Drivers::Detectors
