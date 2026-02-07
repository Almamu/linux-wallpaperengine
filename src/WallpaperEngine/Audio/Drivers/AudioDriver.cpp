#include "AudioDriver.h"

namespace WallpaperEngine::Audio::Drivers {
AudioDriver::AudioDriver (
    Application::ApplicationContext& applicationContext, Detectors::AudioPlayingDetector& detector,
    Recorders::PlaybackRecorder& recorder
) :
    m_applicationContext (applicationContext),
    m_detector (detector),
    m_recorder (recorder) {
    // perform a few update cycles to ensure data is ready before anything actually uses the audio
    this->AudioDriver::update ();
    this->AudioDriver::update ();
}

void AudioDriver::update () {
    this->m_recorder.update ();
    this->m_detector.update ();
}

Application::ApplicationContext& AudioDriver::getApplicationContext () const {
    return this->m_applicationContext;
}

Detectors::AudioPlayingDetector& AudioDriver::getAudioDetector () const {
    return this->m_detector;
}

Recorders::PlaybackRecorder& AudioDriver::getRecorder () const {
    return this->m_recorder;
}
} // namespace WallpaperEngine::Audio::Drivers
