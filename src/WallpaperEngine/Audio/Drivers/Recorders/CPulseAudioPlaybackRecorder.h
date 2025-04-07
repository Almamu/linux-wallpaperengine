#pragma once

#include "kiss_fftr.h"
#include "CPlaybackRecorder.h"
#include <pulse/pulseaudio.h>

#define WAVE_BUFFER_SIZE 1024

namespace WallpaperEngine::Audio::Drivers::Recorders {
class CPlaybackRecorder;

class CPulseAudioPlaybackRecorder final : public CPlaybackRecorder {
  public:
    /**
     * Struct that contains all the required data for the PulseAudio callbacks
     */
    struct SPulseAudioData {
        kiss_fftr_cfg kisscfg;
        uint8_t* audioBuffer;
        uint8_t* audioBufferTmp;
        size_t currentWritePointer;
        bool fullFrameReady;
        pa_stream* captureStream;
    };

    CPulseAudioPlaybackRecorder ();
    ~CPulseAudioPlaybackRecorder () override;

    void update () override;

  private:
    pa_mainloop* m_mainloop;
    pa_mainloop_api* m_mainloopApi;
    pa_context* m_context;
    pa_stream* m_captureStream;
    SPulseAudioData m_captureData;

    float m_audioFFTbuffer [WAVE_BUFFER_SIZE];
    kiss_fft_cpx m_FFTinfo [WAVE_BUFFER_SIZE / 2 + 1] = {0};
    float m_FFTdestination64 [64] = {0};
    float m_FFTdestination32 [32] = {0};
    float m_FFTdestination16 [16] = {0};
};
} // namespace WallpaperEngine::Audio::Drivers::Recorders
