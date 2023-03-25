#include "CAudioDriver.h"

namespace WallpaperEngine::Audio::Drivers
{
    CAudioDriver::CAudioDriver (Application::CApplicationContext& applicationContext, Detectors::CAudioPlayingDetector& detector, Recorders::CPlaybackRecorder& recorder) :
        m_applicationContext (applicationContext),
        m_detector (detector),
        m_recorder (recorder)
    {
    }

    void CAudioDriver::update ()
    {
        this->m_recorder.update ();
        this->m_detector.update ();
    }

    Application::CApplicationContext& CAudioDriver::getApplicationContext ()
    {
        return this->m_applicationContext;
    }
    Detectors::CAudioPlayingDetector& CAudioDriver::getAudioDetector ()
    {
        return this->m_detector;
    }

    Recorders::CPlaybackRecorder& CAudioDriver::getRecorder ()
    {
        return this->m_recorder;
    }
}
