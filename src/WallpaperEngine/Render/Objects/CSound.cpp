#include <SDL.h>

#include "CSound.h"

extern bool g_AudioEnabled;

using namespace WallpaperEngine::Render::Objects;

CSound::CSound (CScene* scene, Core::Objects::CSound* sound) :
    CObject (scene, Type, sound),
    m_sound (sound)
{
    if (g_AudioEnabled)
        this->load ();
}

void CSound::load ()
{
    for (const auto& cur : this->m_sound->getSounds ())
    {
        uint32_t filesize = 0;
        const void* filebuffer = this->getContainer ()->readFile (cur, &filesize);

        auto stream = new Audio::CAudioStream (this->getScene ()->getAudioContext (), filebuffer, filesize);

        stream->setRepeat (this->m_sound->isRepeat ());

        this->m_audioStreams.push_back (stream);
        this->m_soundBuffer.push_back (filebuffer);

        // add the stream to the context so it can be played
        this->getScene ()->getAudioContext ().addStream (stream);
    }
}

void CSound::render ()
{

}

const std::string CSound::Type = "sound";