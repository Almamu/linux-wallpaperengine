#include "AudioPlayingDetector.h"

namespace WallpaperEngine::Audio::Drivers::Detectors {
AudioPlayingDetector::AudioPlayingDetector (
    Application::ApplicationContext& appContext,
    const Render::Drivers::Detectors::FullScreenDetector& fullscreenDetector) :
    m_applicationContext (appContext),
    m_fullscreenDetector (fullscreenDetector) {}

bool AudioPlayingDetector::anythingPlaying () const {
    return this->m_isPlaying;
}

Application::ApplicationContext& AudioPlayingDetector::getApplicationContext () const {
    return this->m_applicationContext;
}

const Render::Drivers::Detectors::FullScreenDetector& AudioPlayingDetector::getFullscreenDetector () const {
    return this->m_fullscreenDetector;
}

void AudioPlayingDetector::setIsPlaying (const bool newState) {
    this->m_isPlaying = newState;
}

void AudioPlayingDetector::update () {}

} // namespace WallpaperEngine::Audio::Drivers::Detectors