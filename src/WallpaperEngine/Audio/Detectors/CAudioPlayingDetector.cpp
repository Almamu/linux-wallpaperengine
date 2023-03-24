#include "CAudioPlayingDetector.h"

namespace WallpaperEngine::Audio::Detectors
{
    CAudioPlayingDetector::CAudioPlayingDetector (Application::CApplicationContext& appContext) :
        m_applicationContext (appContext)
    {
    }

    Application::CApplicationContext& CAudioPlayingDetector::getApplicationContext ()
    {
        return this->m_applicationContext;
    }
}