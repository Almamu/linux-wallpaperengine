#include "AudioDriver.h"

namespace WallpaperEngine::Audio::Drivers {
AudioDriver::AudioDriver (Context& applicationContext) : m_context (applicationContext) {}

void AudioDriver::update () {
}

Context& AudioDriver::getContext () const { return this->m_context; }

} // namespace WallpaperEngine::Audio::Drivers
