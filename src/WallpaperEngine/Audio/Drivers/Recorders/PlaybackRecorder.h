#pragma once

#include <chrono>

namespace WallpaperEngine::Audio::Drivers::Recorders {
class PlaybackRecorder {
public:
    virtual ~PlaybackRecorder () = default;

    virtual void update ();

    /**
     * Publish one raw Wallpaper Engine-compatible 64-band stereo spectrum and update the
     * normalized scene spectra. Alternative capture backends can use the same processing
     * path without duplicating the scene renderer's envelope and smoothing behavior.
     */
    void setRawSpectrum (const float* left, const float* right, float deltaSeconds);

    // The web API consumes the capture processor's raw 64-band output.
    float rawAudio64Left[64] = { 0 };
    float rawAudio64Right[64] = { 0 };

    // Scene shaders and SceneScript consume the normalized, smoothed spectra.
    float audio16Left[16] = { 0 };
    float audio16Right[16] = { 0 };
    float audio16Average[16] = { 0 };
    float audio32Left[32] = { 0 };
    float audio32Right[32] = { 0 };
    float audio32Average[32] = { 0 };
    float audio64Left[64] = { 0 };
    float audio64Right[64] = { 0 };
    float audio64Average[64] = { 0 };

protected:
    [[nodiscard]] float elapsedUpdateSeconds ();

private:
    float m_peakEnvelope[2][8] = { { 0 } };
    float m_intermediateSpectrum[2][64] = { { 0 } };
    std::chrono::steady_clock::time_point m_lastUpdate {};
};
} // namespace WallpaperEngine::Audio::Drivers::Recorders
