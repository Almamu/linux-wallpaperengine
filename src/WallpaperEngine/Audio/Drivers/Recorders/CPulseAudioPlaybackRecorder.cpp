#include "CPulseAudioPlaybackRecorder.h"
#include "WallpaperEngine/Logging/CLog.h"
#include <cmath>
#include <cstring>
#include <glm/common.hpp>

float movetowards (float current, float target, float maxDelta) {
    if (abs (target - current) <= maxDelta)
        return target;

    return current + glm::sign (target - current) * maxDelta;
}

namespace WallpaperEngine::Audio::Drivers::Recorders {
void pa_stream_notify_cb (pa_stream* stream, void* /*userdata*/) {
    switch (pa_stream_get_state (stream)) {
        case PA_STREAM_FAILED: sLog.error ("Cannot open stream for capture. Audio processing is disabled"); break;
        case PA_STREAM_READY: sLog.debug ("Capture stream ready"); break;
        default: sLog.debug("pa_stream_get_state unknown result"); break;
    }
}

void pa_stream_read_cb (pa_stream* stream, const size_t /*nbytes*/, void* userdata) {
    auto* recorder = static_cast<CPulseAudioPlaybackRecorder::SPulseAudioData*> (userdata);

    // Careful when to pa_stream_peek() and pa_stream_drop()!
    // c.f. https://www.freedesktop.org/software/pulseaudio/doxygen/stream_8h.html#ac2838c449cde56e169224d7fe3d00824
    const uint8_t* data = nullptr;
    size_t currentSize;
    if (pa_stream_peek (stream, reinterpret_cast<const void**> (&data), &currentSize) != 0) {
        sLog.error ("Failed to peek at stream data...");
        return;
    }

    if (data == nullptr && currentSize == 0) {
        // No data in the buffer, ignore.
        return;
    }

    if (data == nullptr && currentSize > 0) {
        // Hole in the buffer. We must drop it.
        if (pa_stream_drop (stream) != 0) {
            sLog.error ("Failed to drop a hole while capturing!");
            return;
        }
    } else if (currentSize > 0 && data) {
        size_t dataToCopy = std::min (currentSize, WAVE_BUFFER_SIZE - recorder->currentWritePointer);

        // depending on the amount of data available, we might want to read one or multiple frames
        size_t end = recorder->currentWritePointer + dataToCopy;

        // this packet will fill the buffer, perform some extra checks for extra full buffers and get the latest one
        if (end == WAVE_BUFFER_SIZE) {
            size_t numberOfFullBuffers = (currentSize - dataToCopy) / WAVE_BUFFER_SIZE;

            if (numberOfFullBuffers > 0) {
                // calculate the start of the last block (we need the end of the previous block, hence the - 1)
                size_t startOfLastBuffer = std::max(dataToCopy + (numberOfFullBuffers - 1) * WAVE_BUFFER_SIZE, currentSize - WAVE_BUFFER_SIZE);
                // copy directly into the final buffer
                memcpy (recorder->audioBuffer, &data [startOfLastBuffer], WAVE_BUFFER_SIZE * sizeof (uint8_t));
                // copy whatever is left to the read/write buffer
                recorder->currentWritePointer = currentSize - startOfLastBuffer - WAVE_BUFFER_SIZE;
                memcpy (recorder->audioBufferTmp, &data [startOfLastBuffer + WAVE_BUFFER_SIZE], recorder->currentWritePointer * sizeof (uint8_t));
            } else {
                // okay, no full extra packets available, copy the rest of the data and flip the buffers
                memcpy (&recorder->audioBufferTmp [recorder->currentWritePointer], data, dataToCopy * sizeof (uint8_t));
                uint8_t* tmp = recorder->audioBuffer;
                recorder->audioBuffer = recorder->audioBufferTmp;
                recorder->audioBufferTmp = tmp;
                // reset write pointer
                recorder->currentWritePointer = 0;
            }

            // signal a new frame is ready
            recorder->fullFrameReady = true;
        } else {
            // copy over available data to the tmp buffer and everything should be set
            memcpy (&recorder->audioBufferTmp [recorder->currentWritePointer], data, dataToCopy * sizeof (uint8_t));
            recorder->currentWritePointer += dataToCopy;
        }
    }

    if (pa_stream_drop (stream) != 0) {
        sLog.error ("Failed to drop data after peeking");
    }
}

void pa_server_info_cb (pa_context* ctx, const pa_server_info* info, void* userdata) {
    auto* recorder = static_cast<CPulseAudioPlaybackRecorder::SPulseAudioData*> (userdata);

    pa_sample_spec spec;
    spec.format = PA_SAMPLE_U8;
    spec.rate = 44100;
    spec.channels = 1;

    if (recorder->captureStream) {
        pa_stream_unref (recorder->captureStream);
    }

    recorder->captureStream = pa_stream_new (ctx, "output monitor", &spec, nullptr);

    pa_stream_set_state_callback (recorder->captureStream, &pa_stream_notify_cb, userdata);
    pa_stream_set_read_callback (recorder->captureStream, &pa_stream_read_cb, userdata);

    std::string monitor_name (info->default_sink_name);
    monitor_name += ".monitor";

    // setup latency
    pa_buffer_attr attr {};

    // 10 = latency msecs, 750 = max msecs to store
    size_t bytesPerSec = pa_bytes_per_second (&spec);
    attr.fragsize = bytesPerSec * 10 / 100;
    attr.maxlength = attr.fragsize + bytesPerSec * 750 / 100;

    if (pa_stream_connect_record (recorder->captureStream, monitor_name.c_str (), &attr, PA_STREAM_ADJUST_LATENCY) != 0) {
        sLog.error ("Failed to connect to input for recording");
    }
}

void pa_context_subscribe_cb (pa_context* ctx, pa_subscription_event_type_t t, uint32_t idx, void* userdata) {
    // sink changes mean re-take the stream
    pa_context_get_server_info (ctx, &pa_server_info_cb, userdata);
}

void pa_context_notify_cb (pa_context* ctx, void* userdata) {
    switch (pa_context_get_state (ctx)) {
        case PA_CONTEXT_READY: {
            // set callback
            pa_context_set_subscribe_callback (ctx, pa_context_subscribe_cb, userdata);
            // set events mask and enable event callback.
            pa_operation* o = pa_context_subscribe (
                ctx, static_cast<pa_subscription_mask_t> (PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SOURCE),
                nullptr, nullptr);

            if (o)
                pa_operation_unref (o);

            // context being ready means to fetch the sink too
            pa_context_get_server_info (ctx, &pa_server_info_cb, userdata);

            break;
        }
        case PA_CONTEXT_FAILED:
            sLog.error ("PulseAudio context initialization failed. Audio processing is disabled");
            break;
        default:
            sLog.debug ("pa_context_get_state unknown result");
            break;
    }
}

CPulseAudioPlaybackRecorder::CPulseAudioPlaybackRecorder () :
    m_captureData({
        .kisscfg = kiss_fftr_alloc (WAVE_BUFFER_SIZE, 0, nullptr, nullptr),
        .audioBuffer = new uint8_t [WAVE_BUFFER_SIZE],
        .audioBufferTmp = new uint8_t [WAVE_BUFFER_SIZE]
    }) {
    this->m_mainloop = pa_mainloop_new ();
    this->m_mainloopApi = pa_mainloop_get_api (this->m_mainloop);
    this->m_context = pa_context_new (this->m_mainloopApi, "wallpaperengine-audioprocessing");

    pa_context_set_state_callback (this->m_context, &pa_context_notify_cb, &this->m_captureData);

    if (pa_context_connect (this->m_context, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
        sLog.error ("PulseAudio connection failed! Audio processing is disabled");
        return;
    }

    // wait until the context is ready
    while (pa_context_get_state (this->m_context) != PA_CONTEXT_READY)
        pa_mainloop_iterate (this->m_mainloop, 1, nullptr);
}

CPulseAudioPlaybackRecorder::~CPulseAudioPlaybackRecorder () {
    delete [] this->m_captureData.audioBufferTmp;
    delete [] this->m_captureData.audioBuffer;
    free (this->m_captureData.kisscfg);

    pa_context_disconnect (this->m_context);
    pa_mainloop_free (this->m_mainloop);
}

void CPulseAudioPlaybackRecorder::update () {
    pa_mainloop_iterate (this->m_mainloop, 0, nullptr);

    // interpolate current values to the destination
    for (int i = 0; i < 64; i++) {
        this->audio64 [i] = movetowards (this->audio64 [i], this->m_FFTdestination64 [i], 0.3f);
        if (i >= 32)
            continue;
        this->audio32 [i] = movetowards (this->audio32 [i], this->m_FFTdestination32 [i], 0.3f);
        if (i >= 16)
            continue;
        this->audio16 [i] = movetowards (this->audio16 [i], this->m_FFTdestination16 [i], 0.3f);
    }

    if (!this->m_captureData.fullFrameReady)
        return;

    this->m_captureData.fullFrameReady = false;

    // convert audio data to deltas so the fft library can properly handle it
    for (int i = 0; i < WAVE_BUFFER_SIZE; i ++) {
        this->m_audioFFTbuffer [i] = (this->m_captureData.audioBuffer[i] - 128) / 128.0f;
    }

    // perform full fft pass
    kiss_fftr (this->m_captureData.kisscfg, this->m_audioFFTbuffer, this->m_FFTinfo);

    // now reduce to the different bands
    // use just one for loop to produce all 3
    for (int band = 0; band < 64; band ++) {
        int index = band * 2;
        float f1 = this->m_FFTinfo[index].r;
        float f2 = this->m_FFTinfo[index].i;
        f2 = f1 * f1 + f2 * f2; // magnitude
        f1 = 0.0f;

        if (f2 > 0.0f) {
            f1 = 0.35f * log10 (f2);
        }

        this->m_FFTdestination64 [band] = fmin (1.0f, f1 * static_cast<float> (2.0f - pow (M_E, (1.0f - band / 63.0f) * 1.0f - 0.5f)));
        this->m_FFTdestination32 [band >> 1] = fmin (1.0f, f1 * static_cast<float> (2.0f - pow (M_E, (1.0f - band / 31.0f) * 1.0f - 0.5f)));
        this->m_FFTdestination16 [band >> 2] = fmin (1.0f, f1 * static_cast<float> (2.0f - pow (M_E, (1.0f - band / 15.0f) * 1.0f - 0.5f)));
    }
}

} // namespace WallpaperEngine::Audio::Drivers::Recorders