#include "AudioContext.h"
#include "WallpaperEngine/Audio/Drivers/AudioDriver.h"

namespace WallpaperEngine::Audio {
AudioContext::AudioContext (Drivers::AudioDriver& driver) : m_driver (driver) {}

void AudioContext::addStream (AudioStream* stream) const {
    this->m_driver.addStream (stream);
}

AVSampleFormat AudioContext::getFormat () const {
    return this->m_driver.getFormat ();
}

int AudioContext::getSampleRate () const {
    return this->m_driver.getSampleRate ();
}

int AudioContext::getChannels () const {
    return this->m_driver.getChannels ();
}

Application::ApplicationContext& AudioContext::getApplicationContext () const {
    return this->m_driver.getApplicationContext ();
}

Drivers::Recorders::PlaybackRecorder& AudioContext::getRecorder () const {
    return this->m_driver.getRecorder ();
}
} // namespace WallpaperEngine::Audio