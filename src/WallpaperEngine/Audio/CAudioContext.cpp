#include "CAudioContext.h"
#include "WallpaperEngine/Audio/Drivers/CAudioDriver.h"

namespace WallpaperEngine::Audio
{
    CAudioContext::CAudioContext (Drivers::CAudioDriver& driver) :
        m_driver (driver)
    {
    }

    void CAudioContext::addStream (CAudioStream* stream)
    {
        this->m_driver.addStream (stream);
    }

    AVSampleFormat CAudioContext::getFormat () const
    {
        return this->m_driver.getFormat ();
    }

    int CAudioContext::getSampleRate () const
    {
        return this->m_driver.getSampleRate ();
    }

    int CAudioContext::getChannels () const
    {
        return this->m_driver.getChannels ();
    }

    Application::CApplicationContext& CAudioContext::getApplicationContext ()
    {
        return this->m_driver.getApplicationContext ();
    }
}