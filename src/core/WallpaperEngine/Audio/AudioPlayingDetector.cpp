#include "AudioPlayingDetector.h"
#include "WallpaperEngine/Context.h"

namespace WallpaperEngine::Audio {
AudioPlayingDetector::AudioPlayingDetector (Context& context) : m_context (context) { }

bool AudioPlayingDetector::anythingPlaying () const { return this->m_isPlaying; }

void AudioPlayingDetector::update () {
	if (this->m_context.config.mute_check == nullptr) {
		return;
	}

	if (this->m_context.config.mute_check->is_muted == nullptr) {
		return;
	}

	this->m_isPlaying = this->m_context.config.mute_check->is_muted (this->m_context.config.mute_check->user_parameter);
}

} // namespace WallpaperEngine::Audio