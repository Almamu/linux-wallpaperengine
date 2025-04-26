#include <SDL.h>

#include "CSound.h"

using namespace WallpaperEngine::Render::Objects;

CSound::CSound (Wallpapers::CScene* scene, const Core::Objects::CSound* sound) :
    CObject (scene, sound),
    m_sound (sound) {
    if (this->getContext ().getApp ().getContext ().settings.audio.enabled)
        this->load ();
}

CSound::~CSound() {
    // free all the sound buffers and streams
    for (const auto& stream : this->m_audioStreams) {
        delete stream;
    }

    this->m_soundBuffer.clear ();
}

void CSound::load () {
    for (const auto& cur : this->m_sound->getSounds ()) {
        uint32_t filesize = 0;
        std::shared_ptr<const uint8_t[]> filebuffer = this->getContainer ()->readFile (cur, &filesize);

        auto stream = new Audio::CAudioStream (this->getScene ()->getAudioContext (), filebuffer, filesize);

        stream->setRepeat (this->m_sound->isRepeat ());

        this->m_audioStreams.push_back (stream);
        this->m_soundBuffer.push_back (filebuffer);

        // add the stream to the context so it can be played
        this->getScene ()->getAudioContext ().addStream (stream);
    }
}

void CSound::render () {}
