#pragma once

#include "PlaybackRecorder.h"
#include "kiss_fftr.h"
#include <pulse/pulseaudio.h>

#define WAVE_BUFFER_SIZE 1024

namespace WallpaperEngine::Audio::Drivers::Recorders {
class PlaybackRecorder;

class PulseAudioPlaybackRecorder final : public PlaybackRecorder {
  public:
    /**
     * Struct that contains all the required data for the PulseAudio callbacks
     */
    struct PulseAudioData {
        kiss_fftr_cfg kisscfg;
        uint8_t* audioBuffer;
        uint8_t* audioBufferTmp;
        size_t currentWritePointer;
        bool fullFrameReady;
        pa_stream* captureStream;
    };

    PulseAudioPlaybackRecorder ();
    ~PulseAudioPlaybackRecorder () override;

    void update () override;

  private:
    pa_mainloop* m_mainloop;
    pa_mainloop_api* m_mainloopApi;
    pa_context* m_context;
    PulseAudioData m_captureData;

    float m_audioFFTbuffer [WAVE_BUFFER_SIZE] = {0.0f};
    kiss_fft_cpx m_FFTinfo [WAVE_BUFFER_SIZE / 2 + 1] = {
        {.r = 0.0f, .i = 0.0f}
    };
    float m_FFTdestination64 [64] = {0};
    float m_FFTdestination32 [32] = {0};
    float m_FFTdestination16 [16] = {0};
};
} // namespace WallpaperEngine::Audio::Drivers::Recorders
