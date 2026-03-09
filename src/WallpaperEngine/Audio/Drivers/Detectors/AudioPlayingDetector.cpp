#include "AudioPlayingDetector.h"

namespace WallpaperEngine::Audio::Drivers::Detectors {
AudioPlayingDetector::AudioPlayingDetector (
	wp_mute_check& source, std::unique_ptr<Render::Drivers::Detectors::FullScreenDetector> fullscreenDetector
) : m_source (source), m_fullscreenDetector (std::move (fullscreenDetector)) { }

bool AudioPlayingDetector::anythingPlaying () const { return this->m_isPlaying; }

const Render::Drivers::Detectors::FullScreenDetector& AudioPlayingDetector::getFullscreenDetector () const {
	return *this->m_fullscreenDetector.get ();
}

void AudioPlayingDetector::update () { this->m_isPlaying = this->m_source.is_muted (this->m_source.user_parameter); }

} // namespace WallpaperEngine::Audio::Drivers::Detectors