#include "CAudioPlayingDetector.h"

namespace WallpaperEngine::Audio::Drivers::Detectors
{
    CAudioPlayingDetector::CAudioPlayingDetector (
        Application::CApplicationContext& appContext,
        Render::Drivers::Detectors::CFullScreenDetector& fullscreenDetector) :
        m_applicationContext (appContext),
        m_fullscreenDetector (fullscreenDetector),
        m_isPlaying (false)
    {
    }

    bool CAudioPlayingDetector::anythingPlaying () const
    {
        return this->m_isPlaying;
    }

    Application::CApplicationContext& CAudioPlayingDetector::getApplicationContext ()
    {
        return this->m_applicationContext;
    }

    Render::Drivers::Detectors::CFullScreenDetector& CAudioPlayingDetector::getFullscreenDetector ()
    {
        return this->m_fullscreenDetector;
    }

    void CAudioPlayingDetector::setIsPlaying (bool newState)
    {
        this->m_isPlaying = newState;
    }
}