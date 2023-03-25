#include <cstring>
#include <cmath>
#include <glm/common.hpp>
#include "CPulseAudioPlaybackRecorder.h"
#include "WallpaperEngine/Logging/CLog.h"
#include "External/Android/fft.h"

namespace WallpaperEngine::Audio::Drivers::Recorders
{
    float movetowards(float current, float target, float maxDelta)
    {
        if (abs(target - current) <= maxDelta)
            return target;

        return current + glm::sign(target - current) * maxDelta;
    }

    void pa_stream_notify_cb(pa_stream *stream, void* /*userdata*/)
    {
        const pa_stream_state state = pa_stream_get_state(stream);
        switch (state) {
            case PA_STREAM_FAILED:
                sLog.error ("Cannot open stream for capture. Audio processing is disabled");
                break;
            case PA_STREAM_READY:
                sLog.debug ("Capture stream ready");
                break;
        }
    }

    void pa_stream_read_cb(pa_stream *stream, const size_t /*nbytes*/, void* userdata)
    {
        auto* recorder = reinterpret_cast<CPulseAudioPlaybackRecorder*>(userdata);

        // Careful when to pa_stream_peek() and pa_stream_drop()!
        // c.f. https://www.freedesktop.org/software/pulseaudio/doxygen/stream_8h.html#ac2838c449cde56e169224d7fe3d00824
        uint8_t *data = nullptr;
        size_t currentSize;
        if (pa_stream_peek(stream, (const void**)&data, &currentSize) != 0) {
            sLog.error ("Failed to peek at stream data...");
            return;
        }

        if (data == nullptr && currentSize == 0) {
            // No data in the buffer, ignore.
            return;
        } else if (data == nullptr && currentSize > 0) {
            // Hole in the buffer. We must drop it.
            if (pa_stream_drop(stream) != 0) {
                sLog.error ("Failed to drop a hole while capturing!");
                return;
            }
        } else if (currentSize > 0 && data) {
            size_t dataToCopy = std::min (currentSize, WAVE_BUFFER_SIZE - recorder->currentWritePointer);

            memcpy (&recorder->audio_buffer_tmp [recorder->currentWritePointer], data, dataToCopy * sizeof (uint8_t));

            recorder->currentWritePointer += dataToCopy;

            if (recorder->currentWritePointer == WAVE_BUFFER_SIZE) {
                // copy to the final buffer
                memcpy (recorder->audio_buffer, recorder->audio_buffer_tmp, WAVE_BUFFER_SIZE * sizeof (uint8_t));
                // reset the write pointer
                recorder->currentWritePointer = 0;
                recorder->fullframeReady = true;
            }

            // any data read left?
            if (dataToCopy < currentSize) {
                while ((currentSize - dataToCopy) > WAVE_BUFFER_SIZE)
                    dataToCopy += WAVE_BUFFER_SIZE; // there's more than one full frame available, skip it entirely

                // data pending, keep it in the buffer
                memcpy (recorder->audio_buffer_tmp, data + dataToCopy, (currentSize - dataToCopy) * sizeof (uint8_t));

                recorder->currentWritePointer = currentSize - dataToCopy;
            }
        }

        if (pa_stream_drop(stream) != 0) {
            sLog.error ("Failed to drop data after peeking");
        }
    }

    void pa_server_info_cb(pa_context *ctx, const pa_server_info *info, void* userdata)
    {
        auto* recorder = reinterpret_cast<CPulseAudioPlaybackRecorder*>(userdata);

        pa_sample_spec spec;
        spec.format = PA_SAMPLE_U8;
        spec.rate = 44100;
        spec.channels = 1;

        if (recorder->getCaptureStream ())
            pa_stream_unref (recorder->getCaptureStream ());

        pa_stream* captureStream = pa_stream_new(ctx, "output monitor", &spec, nullptr);

        pa_stream_set_state_callback(captureStream, &pa_stream_notify_cb, userdata);
        pa_stream_set_read_callback(captureStream, &pa_stream_read_cb, userdata);

        std::string monitor_name(info->default_sink_name);
        monitor_name += ".monitor";
        if (pa_stream_connect_record(captureStream, monitor_name.c_str(), nullptr, PA_STREAM_NOFLAGS) != 0) {
            sLog.error ("Failed to connect to input for recording");
            return;
        }

        recorder->setCaptureStream (captureStream);
    }

    void pa_context_subscribe_cb (pa_context *ctx, pa_subscription_event_type_t t, uint32_t idx, void *userdata)
    {
        // sink changes mean re-take the stream
        pa_context_get_server_info(ctx, &pa_server_info_cb, userdata);
    }

    void pa_context_notify_cb(pa_context *ctx, void* userdata)
    {
        const pa_context_state state = pa_context_get_state(ctx);
        switch (state) {
            case PA_CONTEXT_READY:
            {
                //set callback
                pa_context_set_subscribe_callback (ctx, pa_context_subscribe_cb, userdata);
                //set events mask and enable event callback.
                pa_operation* o = pa_context_subscribe (
                    ctx, static_cast<pa_subscription_mask_t>(PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SOURCE),
                    NULL, NULL
                );

                if (o)
                    pa_operation_unref (o);

                // context being ready means to fetch the sink too
                pa_context_get_server_info(ctx, &pa_server_info_cb, userdata);

                break;
            }
            case PA_CONTEXT_FAILED:
                sLog.error ("PulseAudio context initialization failed. Audio processing is disabled");
                break;
        }
    }

    CPulseAudioPlaybackRecorder::CPulseAudioPlaybackRecorder ()
    {
        this->m_mainloop = pa_mainloop_new ();
        this->m_mainloopApi = pa_mainloop_get_api (this->m_mainloop);
        this->m_context = pa_context_new (this->m_mainloopApi, "wallpaperengine-audioprocessing");

        pa_context_set_state_callback (this->m_context, &pa_context_notify_cb, this);

        if (pa_context_connect(this->m_context, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
            sLog.error ("PulseAudio connection failed! Audio processing is disabled");
            return;
        }

        // wait until the context is ready
        while (pa_context_get_state (this->m_context) != PA_CONTEXT_READY)
            pa_mainloop_iterate (this->m_mainloop, 1, nullptr);
    }

    CPulseAudioPlaybackRecorder::~CPulseAudioPlaybackRecorder ()
    {
        pa_context_disconnect(this->m_context);
        pa_mainloop_free(this->m_mainloop);
    }

    pa_stream* CPulseAudioPlaybackRecorder::getCaptureStream ()
    {
        return this->m_captureStream;
    }

    void CPulseAudioPlaybackRecorder::setCaptureStream (pa_stream* stream)
    {
        this->m_captureStream = stream;
    }

    void CPulseAudioPlaybackRecorder::update ()
    {
        pa_mainloop_iterate (this->m_mainloop, 1, nullptr);

        // interpolate current values to the destination
        for (int i = 0; i < 64; i ++) {
            this->audio64 [i] = movetowards (this->audio64[i], fft_destination64[i], 0.1f);
            if (i >= 32)
                continue;
            this->audio32 [i] = movetowards (this->audio32[i], fft_destination32[i], 0.1f);
            if (i >= 16)
                continue;
            this->audio16 [i] = movetowards (this->audio16[i], fft_destination16[i], 0.1f);
        }

        if (this->fullframeReady == false)
            return;

        this->fullframeReady = false;

        External::Android::doFft (audio_fft, audio_buffer);

        for (int i = 0; i < 64; i ++) {
            int paramInt = (i + 2) * 2;
            float f1 = audio_fft[paramInt];
            float f2 = audio_fft[paramInt + 1];
            f2 = f1 * f1 + f2 * f2;
            f1 = 0.0F;
            if (f2 > 0.0F)
                f1 = 0.35F * (float)log10(f2);

            this->fft_destination64[i] = fmin(1.0F, f1 * (float)(2.0f - pow(M_E, (1.0F - i / 63.0F) * 1.0f - 0.5f)));
            this->fft_destination32[i >> 1] = fmin(1.0F, f1 * (float)(2.0f - pow(M_E, (1.0F - i / 31.0F) * 1.0f - 0.5f)));
            this->fft_destination16[i >> 2] = fmin(1.0F, f1 * (float)(2.0f - pow(M_E, (1.0F - i / 15.0F) * 1.0f - 0.5f)));
        }
    }
}