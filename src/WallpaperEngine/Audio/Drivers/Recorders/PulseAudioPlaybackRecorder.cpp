#include "PulseAudioPlaybackRecorder.h"
#include "WallpaperEngine/Logging/Log.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace WallpaperEngine::Audio::Drivers::Recorders {
namespace {
    void disconnectCaptureWhenReady (pa_stream* stream, void* /*userdata*/) {
	if (pa_stream_get_state (stream) == PA_STREAM_READY) {
	    pa_stream_set_state_callback (stream, nullptr, nullptr);
	    pa_stream_disconnect (stream);
	}
    }

    void releaseCaptureStream (pa_stream* stream) {
	const auto state = pa_stream_get_state (stream);
	pa_stream_set_read_callback (stream, nullptr, nullptr);
	// A creating stream cannot disconnect until it has a server-side channel.
	// PulseAudio keeps it alive after unref, so retire it on READY without recorder state.
	pa_stream_set_state_callback (stream, state == PA_STREAM_CREATING ? &disconnectCaptureWhenReady : nullptr, nullptr);
	if (state == PA_STREAM_READY) {
	    pa_stream_disconnect (stream);
	}
	pa_stream_unref (stream);
    }

    void appendCaptureFrames (
	PulseAudioPlaybackRecorder::PulseAudioData& recorder, const float* data, std::size_t frameCount
    ) {
	const std::size_t channelCount = recorder.channelCount;
	if (channelCount == 0 || recorder.frameCount == 0
	    || recorder.audioBuffer.size () != recorder.frameCount * channelCount
	    || recorder.writeBuffer.size () != recorder.frameCount * channelCount) {
	    return;
	}

	// The native recorder releases the packet once the active PCM window is full.
	const std::size_t framesToCopy = std::min (frameCount, recorder.frameCount - recorder.currentWriteFrame);
	float* destination = recorder.writeBuffer.data () + recorder.currentWriteFrame * channelCount;
	const std::size_t samplesToCopy = framesToCopy * channelCount;
	if (data == nullptr) {
	    std::fill_n (destination, samplesToCopy, 0.0f);
	} else {
	    std::memcpy (destination, data, samplesToCopy * sizeof (float));
	}

	recorder.currentWriteFrame += framesToCopy;
	if (recorder.currentWriteFrame == recorder.frameCount) {
	    recorder.audioBuffer.swap (recorder.writeBuffer);
	    recorder.currentWriteFrame = 0;
	    recorder.fullFrameReady = true;
	}
    }
} // namespace

void pa_stream_notify_cb (pa_stream* stream, void* /*userdata*/) {
    switch (pa_stream_get_state (stream)) {
	case PA_STREAM_FAILED:
	    sLog.error ("Cannot open stream for capture. Audio processing is disabled");
	    break;
	case PA_STREAM_READY:
	    sLog.debug ("Capture stream ready");
	    break;
	default:
	    break;
    }
}

void pa_stream_read_cb (pa_stream* stream, const size_t /*nbytes*/, void* userdata) {
    auto* recorder = static_cast<PulseAudioPlaybackRecorder::PulseAudioData*> (userdata);

    const void* packet = nullptr;
    size_t currentSize = 0;
    if (pa_stream_peek (stream, &packet, &currentSize) != 0) {
	sLog.error ("Failed to peek at stream data");
	return;
    }

    if (packet == nullptr && currentSize == 0) {
	return;
    }

    const std::size_t bytesPerFrame = sizeof (float) * recorder->channelCount;
    if (bytesPerFrame != 0) {
	const std::size_t frameCount = currentSize / bytesPerFrame;
	appendCaptureFrames (*recorder, static_cast<const float*> (packet), frameCount);
    }

    if (pa_stream_drop (stream) != 0) {
	sLog.error (packet == nullptr ? "Failed to drop a hole while capturing" : "Failed to drop captured data");
    }
}

void pa_server_info_cb (pa_context* ctx, const pa_server_info* info, void* userdata) {
    if (info == nullptr || info->default_sink_name == nullptr) {
	return;
    }

    auto* recorder = static_cast<PulseAudioPlaybackRecorder::PulseAudioData*> (userdata);
    pa_sample_spec spec {};
    spec.format = PA_SAMPLE_FLOAT32NE;
    spec.rate = info->sample_spec.rate == 0 ? 44100 : info->sample_spec.rate;
    spec.channels = static_cast<uint8_t> (std::clamp<unsigned int> (info->sample_spec.channels, 1, 2));

    std::string monitorName (info->default_sink_name);
    monitorName += ".monitor";
    if (recorder->captureStream != nullptr) {
	if (PA_STREAM_IS_GOOD (pa_stream_get_state (recorder->captureStream)) && recorder->monitorName == monitorName
	    && recorder->sampleRate == spec.rate && recorder->channelCount == spec.channels) {
	    return;
	}
	releaseCaptureStream (recorder->captureStream);
    }

    recorder->monitorName = monitorName;
    recorder->sampleRate = spec.rate;
    recorder->channelCount = spec.channels;
    recorder->frameCount = WallpaperEngineSpectrumAnalyzer::windowForSampleRate (spec.rate).pcmFrames;
    recorder->currentWriteFrame = 0;
    recorder->fullFrameReady = false;
    recorder->audioBuffer.assign (recorder->frameCount * recorder->channelCount, 0.0f);
    recorder->writeBuffer.assign (recorder->frameCount * recorder->channelCount, 0.0f);
    recorder->formatGeneration++;

    pa_channel_map channelMap;
    if (spec.channels == 1) {
	pa_channel_map_init_mono (&channelMap);
    } else {
	pa_channel_map_init_stereo (&channelMap);
    }

    recorder->captureStream = pa_stream_new (ctx, "output monitor", &spec, &channelMap);
    if (recorder->captureStream == nullptr) {
	sLog.error ("Failed to create the audio processing capture stream");
	return;
    }

    pa_stream_set_state_callback (recorder->captureStream, &pa_stream_notify_cb, userdata);
    pa_stream_set_read_callback (recorder->captureStream, &pa_stream_read_cb, userdata);

    pa_buffer_attr attributes {};
    const size_t bytesPerSecond = pa_bytes_per_second (&spec);
    attributes.fragsize = static_cast<uint32_t> (bytesPerSecond * 10 / 1000);
    attributes.maxlength = static_cast<uint32_t> (attributes.fragsize + bytesPerSecond * 750 / 1000);

    if (pa_stream_connect_record (recorder->captureStream, monitorName.c_str (), &attributes, PA_STREAM_ADJUST_LATENCY)
	!= 0) {
	sLog.error ("Failed to connect to input for recording");
    }
}

void pa_context_subscribe_cb (
    pa_context* ctx, pa_subscription_event_type_t /*type*/, uint32_t /*index*/, void* userdata
) {
    pa_operation* operation = pa_context_get_server_info (ctx, &pa_server_info_cb, userdata);
    if (operation != nullptr) {
	pa_operation_unref (operation);
    }
}

void pa_context_notify_cb (pa_context* ctx, void* userdata) {
    switch (pa_context_get_state (ctx)) {
	case PA_CONTEXT_READY:
	    {
		pa_context_set_subscribe_callback (ctx, pa_context_subscribe_cb, userdata);
		pa_operation* subscription = pa_context_subscribe (
		    ctx, static_cast<pa_subscription_mask_t> (
			PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SOURCE | PA_SUBSCRIPTION_MASK_SERVER
		    ),
		    nullptr, nullptr
		);
		if (subscription != nullptr) {
		    pa_operation_unref (subscription);
		}

		pa_operation* serverInfo = pa_context_get_server_info (ctx, &pa_server_info_cb, userdata);
		if (serverInfo != nullptr) {
		    pa_operation_unref (serverInfo);
		}
		break;
	    }
	case PA_CONTEXT_FAILED:
	    sLog.error ("PulseAudio context initialization failed. Audio processing is disabled");
	    break;
	default:
	    break;
    }
}

PulseAudioPlaybackRecorder::PulseAudioPlaybackRecorder () {
    this->m_captureData.audioBuffer.assign (this->m_captureData.frameCount * this->m_captureData.channelCount, 0.0f);
    this->m_captureData.writeBuffer.assign (this->m_captureData.frameCount * this->m_captureData.channelCount, 0.0f);

    if (const char* scale = std::getenv ("WPE_AUDIO_SCALE")) {
	char* end = nullptr;
	const float parsed = std::strtof (scale, &end);
	const bool hasNumber = end != scale;
	while (std::isspace (static_cast<unsigned char> (*end))) {
	    ++end;
	}
	if (hasNumber && *end == '\0') {
	    this->m_spectrumAnalyzer.setInputVolume (parsed);
	}
    }

    this->m_mainloop = pa_mainloop_new ();
    if (this->m_mainloop == nullptr) {
	sLog.error ("Failed to create the PulseAudio main loop. Audio processing is disabled");
	return;
    }

    this->m_mainloopApi = pa_mainloop_get_api (this->m_mainloop);
    this->m_context = pa_context_new (this->m_mainloopApi, "wallpaperengine-audioprocessing");
    if (this->m_context == nullptr) {
	sLog.error ("Failed to create the PulseAudio context. Audio processing is disabled");
	return;
    }

    pa_context_set_state_callback (this->m_context, &pa_context_notify_cb, &this->m_captureData);
    if (pa_context_connect (this->m_context, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
	sLog.error ("PulseAudio connection failed. Audio processing is disabled");
	return;
    }

    while (true) {
	const pa_context_state state = pa_context_get_state (this->m_context);
	if (state == PA_CONTEXT_READY) {
	    break;
	}
	if (state == PA_CONTEXT_FAILED || state == PA_CONTEXT_TERMINATED) {
	    return;
	}
	pa_mainloop_iterate (this->m_mainloop, 1, nullptr);
    }
}

PulseAudioPlaybackRecorder::~PulseAudioPlaybackRecorder () {
    if (this->m_captureData.captureStream != nullptr) {
	releaseCaptureStream (this->m_captureData.captureStream);
    }
    if (this->m_context != nullptr) {
	pa_context_disconnect (this->m_context);
	pa_context_unref (this->m_context);
    }
    if (this->m_mainloop != nullptr) {
	pa_mainloop_free (this->m_mainloop);
    }
}

void PulseAudioPlaybackRecorder::update () {
    const float deltaSeconds = this->elapsedUpdateSeconds ();
    if (this->m_mainloop != nullptr) {
	// Drain all queued fragments so capture cannot fall behind the render loop.
	for (int guard = 0; guard < 256 && pa_mainloop_iterate (this->m_mainloop, 0, nullptr) > 0; guard++) { }
    }

    if (this->m_captureData.fullFrameReady) {
	this->m_captureData.fullFrameReady = false;
	if (this->m_configuredGeneration != this->m_captureData.formatGeneration) {
	    this->m_spectrumAnalyzer.configure (this->m_captureData.sampleRate);
	    this->m_configuredGeneration = this->m_captureData.formatGeneration;
	}

	float left[WallpaperEngineSpectrumAnalyzer::BandCount] = { 0 };
	float right[WallpaperEngineSpectrumAnalyzer::BandCount] = { 0 };
	this->m_spectrumAnalyzer.processInterleaved (
	    this->m_captureData.audioBuffer.data (), this->m_captureData.frameCount, this->m_captureData.channelCount,
	    left, right
	);
	this->setRawSpectrum (left, right, deltaSeconds);

	if (std::getenv ("WPE_AUDIO_DEBUG") != nullptr) {
	    static int debugCounter = 0;
	    if (++debugCounter % 23 == 0) {
		double sumSquares = 0.0;
		for (const float sample : this->m_captureData.audioBuffer) {
		    sumSquares += static_cast<double> (sample) * sample;
		}
		const float rms = static_cast<float> (
		    std::sqrt (sumSquares / std::max<std::size_t> (this->m_captureData.audioBuffer.size (), 1))
		);
		if (FILE* file = std::fopen ("/tmp/we-audio-debug.log", "a")) {
		    std::fprintf (
			file, "rms=%.4f l0=%.4f l16=%.4f l32=%.4f l63=%.4f r32=%.4f\n", rms, this->rawAudio64Left[0],
			this->rawAudio64Left[16], this->rawAudio64Left[32], this->rawAudio64Left[63],
			this->rawAudio64Right[32]
		    );
		    std::fclose (file);
		}
	    }
	}
    } else {
	// Wallpaper Engine's scene processor continues following the most recent capture
	// spectrum on every rendered frame, even when no new capture block arrived.
	this->setRawSpectrum (this->rawAudio64Left, this->rawAudio64Right, deltaSeconds);
    }
}
} // namespace WallpaperEngine::Audio::Drivers::Recorders
