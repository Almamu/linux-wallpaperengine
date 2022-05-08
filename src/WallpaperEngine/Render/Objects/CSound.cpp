#include <SDL.h>

#include "CSound.h"

using namespace WallpaperEngine::Render::Objects;

CSound::CSound (CScene* scene, Core::Objects::CSound* sound) :
    CObject (scene, Type, sound),
    m_sound (sound)
{
    this->load ();
}

void CSound::load ()
{
    auto cur = this->m_sound->getSounds ().begin ();
    auto end = this->m_sound->getSounds ().end ();

    for (; cur != end; cur ++)
    {
        uint32_t filesize = 0;
        void* filebuffer = this->getContainer ()->readFile ((*cur), &filesize);

        this->m_audioStreams.push_back (new Audio::CAudioStream (filebuffer, filesize));
        this->m_soundBuffer.push_back (filebuffer);
    }
}

void CSound::render ()
{

}

const std::string CSound::Type = "sound";