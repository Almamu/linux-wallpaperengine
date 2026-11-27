#pragma once

#include "PlaybackRecorder.h"
#include "WallpaperEngineSpectrumAnalyzer.h"
#include <cstdint>
#include <pulse/pulseaudio.h>
#include <string>
#include <vector>

namespace WallpaperEngine::Audio::Drivers::Recorders {
class PlaybackRecorder;

class PulseAudioPlaybackRecorder final : public PlaybackRecorder {
public:
    /**
     * Struct that contains all the required data for the PulseAudio callbacks
     */
    struct PulseAudioData {
	std::vector<float> audioBuffer;
	std::vector<float> writeBuffer;
	std::size_t currentWriteFrame = 0;
	std::size_t frameCount = WallpaperEngineSpectrumAnalyzer::BasePcmFrameCount;
	uint32_t sampleRate = 44100;
	unsigned int channelCount = 2;
	uint64_t formatGeneration = 0;
	bool fullFrameReady = false;
	pa_stream* captureStream = nullptr;
	std::string monitorName;
    };

    PulseAudioPlaybackRecorder ();
    ~PulseAudioPlaybackRecorder () override;

    void update () override;

private:
    pa_mainloop* m_mainloop = nullptr;
    pa_mainloop_api* m_mainloopApi = nullptr;
    pa_context* m_context = nullptr;
    PulseAudioData m_captureData;
    WallpaperEngineSpectrumAnalyzer m_spectrumAnalyzer;
    uint64_t m_configuredGeneration = 0;
};
} // namespace WallpaperEngine::Audio::Drivers::Recorders
