#include "CAudioDriver.h"

namespace WallpaperEngine::Audio::Drivers
{
    CAudioDriver::CAudioDriver (Application::CApplicationContext& applicationContext, Detectors::CAudioPlayingDetector& detector) :
        m_applicationContext (applicationContext),
        m_detector (detector)
    {
    }

    Application::CApplicationContext& CAudioDriver::getApplicationContext ()
    {
        return this->m_applicationContext;
    }
    Detectors::CAudioPlayingDetector& CAudioDriver::getAudioDetector ()
    {
        return this->m_detector;
    }
}
