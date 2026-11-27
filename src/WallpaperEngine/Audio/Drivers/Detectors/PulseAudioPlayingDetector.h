#pragma once

#include "AudioPlayingDetector.h"
#include <condition_variable>
#include <mutex>
#include <pulse/pulseaudio.h>
#include <string>
#include <vector>

namespace WallpaperEngine::Audio::Drivers::Detectors {
class PulseAudioPlayingDetector final : public AudioPlayingDetector {
public:
    explicit PulseAudioPlayingDetector (
	Application::ApplicationContext& appContext, const Render::Drivers::Detectors::FullScreenDetector&
    );
    ~PulseAudioPlayingDetector () override;

    void update () override;

    /** the automute ignore list from settings, reachable from the C callbacks */
    [[nodiscard]] const std::vector<std::string>& getAutomuteIgnore () const;

private:
    void disconnect ();
    pa_mainloop* m_mainloop = nullptr;
    pa_context* m_context = nullptr;
};
} // namespace WallpaperEngine::Audio::Drivers::Detectors
