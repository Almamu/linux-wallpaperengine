#pragma once

#include "AudioPlayingDetector.h"
#include <chrono>
#include <pulse/pulseaudio.h>

namespace WallpaperEngine::Audio::Drivers::Detectors {
class PulseAudioPlayingDetector final : public AudioPlayingDetector {
public:
    explicit PulseAudioPlayingDetector (
	Application::ApplicationContext& appContext, const Render::Drivers::Detectors::FullScreenDetector&
    );
    ~PulseAudioPlayingDetector () override;

    void update () override;

    /**
     * @return If there's a ready connection to the PulseAudio server
     */
    [[nodiscard]] bool isConnected () const;

private:
    /**
     * Starts connecting to the PulseAudio server, update () carries on with it without blocking
     */
    void connect ();
    /**
     * Drops the current connection to the PulseAudio server, if any
     */
    void disconnect ();
    /**
     * Gives up on the current connection, a new one is attempted later
     */
    void connectionFailed ();
    /**
     * Runs the mainloop until done returns true or the timeout passes
     *
     * @return If done returned true before the timeout
     */
    template <typename Predicate> bool iterateUntil (Predicate done, std::chrono::milliseconds timeout);

    pa_mainloop* m_mainloop = nullptr;
    pa_mainloop_api* m_mainloopApi = nullptr;
    pa_context* m_context = nullptr;
    /** When the current connection attempt started */
    std::chrono::steady_clock::time_point m_connectStarted {};
    /** No new connection is attempted before this point, so a missing server isn't retried every frame */
    std::chrono::steady_clock::time_point m_nextConnectAttempt {};
    /** If the server couldn't be reached, so the failure is only logged once */
    bool m_unreachable = false;
};
} // namespace WallpaperEngine::Audio::Drivers::Detectors
