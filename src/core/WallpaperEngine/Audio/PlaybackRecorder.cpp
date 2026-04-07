#include "PlaybackRecorder.h"
#include "WallpaperEngine/Context.h"
#include <glm/common.hpp>

float movetowards (float current, float target, float maxDelta) {
	if (abs (target - current) <= maxDelta) {
		return target;
	}

	return current + glm::sign (target - current) * maxDelta;
}

namespace WallpaperEngine::Audio {
PlaybackRecorder::PlaybackRecorder (Context& context) : m_context (context) {
	this->m_kisscfg = kiss_fftr_alloc (WP_AUDIO_INPUT_FRAME_SIZE, 0, nullptr, nullptr);
}

void PlaybackRecorder::update () {
	// interpolate current values to the destination
	for (int i = 0; i < 64; i++) {
		this->audio64[i] = movetowards (this->audio64[i], this->m_FFTdestination64[i], 0.3f);
		if (i >= 32) {
			continue;
		}
		this->audio32[i] = movetowards (this->audio32[i], this->m_FFTdestination32[i], 0.3f);
		if (i >= 16) {
			continue;
		}
		this->audio16[i] = movetowards (this->audio16[i], this->m_FFTdestination16[i], 0.3f);
	}

	if (this->m_context.audio_input_mix == nullptr) {
		return;
	}

	if (this->m_context.audio_input_mix->is_frame_ready == nullptr) {
		return;
	}

	if (this->m_context.audio_input_mix->get_frame == nullptr) {
		return;
	}

	if (this->m_context.audio_input_mix->is_frame_ready (this->m_context.audio_input_mix->user_parameter) == false) {
		return;
	}

	const unsigned char* buffer
		= this->m_context.audio_input_mix->get_frame (this->m_context.audio_input_mix->user_parameter);

	// convert audio data to deltas so the fft library can properly handle it
	for (int i = 0; i < WP_AUDIO_INPUT_FRAME_SIZE; i++) {
		this->m_audioFFTbuffer[i] = (buffer[i] - 128) / 128.0f;
	}

	// perform full fft pass
	kiss_fftr (this->m_kisscfg, this->m_audioFFTbuffer, this->m_FFTinfo);

	// now reduce to the different bands
	// use just one for loop to produce all 3
	for (int band = 0; band < 64; band++) {
		const int index = band * 2;
		float f1 = this->m_FFTinfo[index].r;
		float f2 = this->m_FFTinfo[index].i;
		f2 = f1 * f1 + f2 * f2; // magnitude
		f1 = 0.0f;

		if (f2 > 0.0f) {
			f1 = 0.35f * log10 (f2);
		}

		this->m_FFTdestination64[band]
			= fmin (1.0f, f1 * static_cast<float> (2.0f - pow (M_E, (1.0f - band / 63.0f) * 1.0f - 0.5f)));
		this->m_FFTdestination32[band >> 1]
			= fmin (1.0f, f1 * static_cast<float> (2.0f - pow (M_E, (1.0f - band / 31.0f) * 1.0f - 0.5f)));
		this->m_FFTdestination16[band >> 2]
			= fmin (1.0f, f1 * static_cast<float> (2.0f - pow (M_E, (1.0f - band / 15.0f) * 1.0f - 0.5f)));
	}
}

} // namespace WallpaperEngine::Audio