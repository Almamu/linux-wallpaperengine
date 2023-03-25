#pragma once

#include <pulse/pulseaudio.h>
#include "External/Android/fft.h"
#include "CPlaybackRecorder.h"

namespace WallpaperEngine::Audio::Drivers::Recorders
{
    class CPlaybackRecorder;

    class CPulseAudioPlaybackRecorder : public CPlaybackRecorder
    {
    public:
        CPulseAudioPlaybackRecorder ();
        ~CPulseAudioPlaybackRecorder ();

        void update () override;

        /**
         * @return The current stream we're capturing from
         */
        [[nodiscard]] pa_stream* getCaptureStream ();

        /**
         * @param stream The new stream to be capturing off from
         */
        void setCaptureStream (pa_stream* stream);

        uint8_t audio_buffer [WAVE_BUFFER_SIZE] = {0x80};
        uint8_t audio_buffer_tmp [WAVE_BUFFER_SIZE] = {0x80};
        uint8_t audio_fft [WAVE_BUFFER_SIZE] = {0};
        size_t currentWritePointer = 0;
        bool fullframeReady = false;

    private:
        pa_mainloop* m_mainloop;
        pa_mainloop_api* m_mainloopApi;
        pa_context* m_context;
        pa_stream* m_captureStream;

        float fft_destination64[64];
        float fft_destination32[32];
        float fft_destination16[16];
    };
}
