#include "AudioDriver.h"

namespace WallpaperEngine::Audio::Drivers {
AudioDriver::AudioDriver (
	Context& applicationContext, std::unique_ptr<Detectors::AudioPlayingDetector> detector,
	std::unique_ptr<Recorders::PlaybackRecorder> recorder
) : m_context (applicationContext), m_detector (std::move (detector)), m_recorder (std::move (recorder)) {
	// perform a few update cycles to ensure data is ready before anything actually uses the audio
	this->AudioDriver::update ();
	this->AudioDriver::update ();
}

void AudioDriver::update () {
	this->m_recorder->update ();
	this->m_detector->update ();
}

Context& AudioDriver::getContext () const { return this->m_context; }

Detectors::AudioPlayingDetector& AudioDriver::getAudioDetector () const { return *this->m_detector.get (); }

Recorders::PlaybackRecorder& AudioDriver::getRecorder () const { return *this->m_recorder.get (); }
} // namespace WallpaperEngine::Audio::Drivers
