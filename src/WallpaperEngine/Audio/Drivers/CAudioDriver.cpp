#include "CAudioDriver.h"

namespace WallpaperEngine::Audio::Drivers
{
    CAudioDriver::CAudioDriver (Application::CApplicationContext& applicationContext) :
        m_applicationContext (applicationContext)
    {
    }

    Application::CApplicationContext& CAudioDriver::getApplicationContext ()
    {
        return this->m_applicationContext;
    }
}
