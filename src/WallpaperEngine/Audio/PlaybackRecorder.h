#pragma once

#include "frontends/context.h"
#include "kiss_fftr.h"

namespace WallpaperEngine {
class Context;
}

namespace WallpaperEngine::Audio {
class PlaybackRecorder final {
public:
	explicit PlaybackRecorder (Context& context);

	void update ();

	float audio16[16] = { 0 };
	float audio32[32] = { 0 };
	float audio64[64] = { 0 };

private:
	Context& m_context;

	kiss_fftr_cfg m_kisscfg;
	float m_audioFFTbuffer[WP_AUDIO_INPUT_FRAME_SIZE] = { 0.0f };
	kiss_fft_cpx m_FFTinfo[WP_AUDIO_INPUT_FRAME_SIZE / 2 + 1] = { { .r = 0.0f, .i = 0.0f } };
	float m_FFTdestination64[64] = { 0 };
	float m_FFTdestination32[32] = { 0 };
	float m_FFTdestination16[16] = { 0 };
};
} // namespace WallpaperEngine::Audio