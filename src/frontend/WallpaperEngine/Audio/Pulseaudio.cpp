#include <cstring>
#include <glm/common.hpp>

#include <pulse/pulseaudio.h>

#include "Pulseaudio.h"
#include "WallpaperEngine/Logging/Log.h"

namespace WallpaperEngine::Audio {
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
	auto* recorder = static_cast<Pulseaudio*> (userdata);

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
		const size_t dataToCopy = std::min (currentSize, WP_AUDIO_INPUT_FRAME_SIZE - recorder->currentWritePointer);

		// depending on the amount of data available, we might want to read one or multiple frames
		const size_t end = recorder->currentWritePointer + dataToCopy;

		// this packet will fill the buffer, perform some extra checks for extra full buffers and get the latest one
		if (end == WP_AUDIO_INPUT_FRAME_SIZE) {
			if (const size_t numberOfFullBuffers = (currentSize - dataToCopy) / WP_AUDIO_INPUT_FRAME_SIZE;
			    numberOfFullBuffers > 0) {
				// calculate the start of the last block (we need the end of the previous block, hence the - 1)
				const size_t startOfLastBuffer = std::max (
					dataToCopy + (numberOfFullBuffers - 1) * WP_AUDIO_INPUT_FRAME_SIZE,
					currentSize - WP_AUDIO_INPUT_FRAME_SIZE
				);
				// copy directly into the final buffer
				memcpy (recorder->audioBuffer, &data[startOfLastBuffer], WP_AUDIO_INPUT_FRAME_SIZE * sizeof (uint8_t));
				// copy whatever is left to the read/write buffer
				recorder->currentWritePointer = currentSize - startOfLastBuffer - WP_AUDIO_INPUT_FRAME_SIZE;
				memcpy (
					recorder->audioBufferTmp, &data[startOfLastBuffer + WP_AUDIO_INPUT_FRAME_SIZE],
					recorder->currentWritePointer * sizeof (uint8_t)
				);
			} else {
				// okay, no full extra packets available, copy the rest of the data and flip the buffers
				memcpy (&recorder->audioBufferTmp[recorder->currentWritePointer], data, dataToCopy * sizeof (uint8_t));
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
			memcpy (&recorder->audioBufferTmp[recorder->currentWritePointer], data, dataToCopy * sizeof (uint8_t));
			recorder->currentWritePointer += dataToCopy;
		}
	}

	if (pa_stream_drop (stream) != 0) {
		sLog.error ("Failed to drop data after peeking");
	}
}

void pa_server_info_cb (pa_context* ctx, const pa_server_info* info, void* userdata) {
	auto* recorder = static_cast<Pulseaudio*> (userdata);

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

	if (pa_stream_connect_record (recorder->captureStream, monitor_name.c_str (), &attr, PA_STREAM_ADJUST_LATENCY)
	    != 0) {
		sLog.error ("Failed to connect to input for recording");
	}
}

void pa_sink_input_info_cb (pa_context* ctx, const pa_sink_input_info* info, int eol, void* userdata) {
	if (info == nullptr || info->proplist == nullptr) {
		return;
	}

	if (eol) {
		return;
	}

	auto impl = static_cast<Pulseaudio*> (userdata);
	const char* value = pa_proplist_gets (info->proplist, PA_PROP_APPLICATION_PROCESS_ID);

	if (value == nullptr) {
		return;
	}

	if (std::stol (value, nullptr, 10) == getpid ()) {
		return;
	}

	if (pa_cvolume_avg (&info->volume) == PA_VOLUME_MUTED) {
		return;
	}

	if (info->corked) {
		return;
	}

	impl->anythingPlaying = true;
}

void pa_context_subscribe_cb (pa_context* ctx, pa_subscription_event_type_t t, uint32_t idx, void* userdata) {
	auto impl = static_cast<Pulseaudio*> (userdata);
	auto facility = t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK;

	if (facility == PA_SUBSCRIPTION_EVENT_SINK_INPUT) {
		impl->anythingPlaying = false;
		pa_context_get_sink_input_info_list (ctx, &pa_sink_input_info_cb, userdata);
	} else if (facility == PA_SUBSCRIPTION_EVENT_SINK || facility == PA_SUBSCRIPTION_EVENT_SOURCE) {
		// context being ready means to fetch the sink too
		pa_context_get_server_info (ctx, &pa_server_info_cb, userdata);
	}
}

void pa_context_notify_cb (pa_context* ctx, void* userdata) {
	switch (pa_context_get_state (ctx)) {
		case PA_CONTEXT_READY:
			{
				// set callback
				pa_context_set_subscribe_callback (ctx, pa_context_subscribe_cb, userdata);
				// set events mask and enable event callback.
				pa_operation* o = pa_context_subscribe (
					ctx,
					static_cast<pa_subscription_mask_t> (
						PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SOURCE | PA_SUBSCRIPTION_MASK_SINK_INPUT
					),
					nullptr, nullptr
				);

				if (o) {
					pa_operation_unref (o);
				}

				// also request sink input status so at startup there's valid data
				pa_context_get_sink_input_info_list (ctx, &pa_sink_input_info_cb, userdata);
				break;
			}
		case PA_CONTEXT_FAILED:
			sLog.error ("PulseAudio context initialization failed. Audio processing is disabled");
			break;
		default:
			break;
	}
}

bool is_frame_ready (void* user_parameter) {
	auto impl = static_cast<Pulseaudio*> (user_parameter);

	// perform a loop on pulse to read any data
	pa_mainloop_iterate (impl->mainloop, 0, nullptr);

	return impl->fullFrameReady;
}

unsigned char* get_frame (void* user_parameter) { return static_cast<Pulseaudio*> (user_parameter)->audioBuffer; }

bool is_muted (void* user_parameter) {
	auto impl = static_cast<Pulseaudio*> (user_parameter);

	// perform a loop on pulse to read any data
	pa_mainloop_iterate (impl->mainloop, 0, nullptr);

	if (impl->desktopEnvironment != nullptr) {
		return impl->anythingPlaying || impl->desktopEnvironment->anything_fullscreen;
	}

	return impl->anythingPlaying;
}

Pulseaudio::Pulseaudio () {
	this->audioBuffer = new uint8_t[WP_AUDIO_INPUT_FRAME_SIZE];
	this->audioBufferTmp = new uint8_t[WP_AUDIO_INPUT_FRAME_SIZE];

	// setup base callbacks for everything
	this->input_mix = {
		.user_parameter = this,
		.is_frame_ready = is_frame_ready,
		.get_frame = get_frame,
	};
	this->mute_check = {
		.user_parameter = this,
		.is_muted = is_muted,
	};
	this->fullFrameReady = false;
	this->anythingPlaying = false;

	this->captureStream = nullptr;
	this->currentWritePointer = 0;
	this->mainloop = pa_mainloop_new ();
	this->mainloopApi = pa_mainloop_get_api (this->mainloop);
	this->context = pa_context_new (this->mainloopApi, "wallpaperengine-audioprocessing");

	pa_context_set_state_callback (this->context, &pa_context_notify_cb, this);

	if (pa_context_connect (this->context, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
		sLog.error ("Failed to connect to PulseAudio");
		return;
	}

	while (pa_context_get_state (this->context) != PA_CONTEXT_READY) {
		pa_mainloop_iterate (this->mainloop, 1, nullptr);
	}
}

Pulseaudio::~Pulseaudio () {
	if (this->captureStream) {
		pa_stream_unref (this->captureStream);
	}

	delete[] this->audioBuffer;
	delete[] this->audioBufferTmp;

	pa_context_disconnect (this->context);
	pa_context_unref (this->context);
	pa_mainloop_free (this->mainloop);
}
}