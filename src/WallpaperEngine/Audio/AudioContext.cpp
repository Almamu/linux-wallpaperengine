#include "AudioContext.h"
#include "WallpaperEngine/Audio/Drivers/AudioDriver.h"
#include "AudioPlayingDetector.h"
#include "PlaybackRecorder.h"

namespace WallpaperEngine::Audio {
AudioContext::AudioContext (
	std::unique_ptr<Drivers::AudioDriver> driver,
	std::unique_ptr<PlaybackRecorder> recorder,
	std::unique_ptr<AudioPlayingDetector> detector
) : m_recorder (std::move(recorder)), m_detector (std::move(detector)), m_driver (std::move (driver)) { }

int AudioContext::addStream (AudioStream* stream) const { return this->m_driver->addStream (stream); }
void AudioContext::removeStream (int streamId) const { this->m_driver->removeStream (streamId); }

AVSampleFormat AudioContext::getFormat () const { return this->m_driver->getFormat (); }

int AudioContext::getSampleRate () const { return this->m_driver->getSampleRate (); }

int AudioContext::getChannels () const { return this->m_driver->getChannels (); }

PlaybackRecorder& AudioContext::getRecorder () const { return *this->m_recorder; }

AudioPlayingDetector& AudioContext::getDetector () const { return *this->m_detector; }

Drivers::AudioDriver& AudioContext::getDriver () const { return *this->m_driver.get (); }
} // namespace WallpaperEngine::Audio