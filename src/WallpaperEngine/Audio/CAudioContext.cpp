#include "CAudioContext.h"
#include "WallpaperEngine/Audio/Drivers/CAudioDriver.h"

using namespace WallpaperEngine::Audio;
using namespace WallpaperEngine::Audio::Drivers;

CAudioContext::CAudioContext (CAudioDriver* driver) :
    m_driver (driver)
{

}

void CAudioContext::addStream (CAudioStream* stream)
{
    this->m_driver->addStream (stream);
}

AVSampleFormat CAudioContext::getFormat () const
{
    return this->m_driver->getFormat ();
}

int CAudioContext::getSampleRate () const
{
    return this->m_driver->getSampleRate ();
}

int CAudioContext::getChannels () const
{
    return this->m_driver->getChannels ();
}