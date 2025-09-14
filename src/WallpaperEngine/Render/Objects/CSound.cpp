#include <SDL.h>

#include "CSound.h"

#include "WallpaperEngine/FileSystem/Container.h"

using namespace WallpaperEngine::Render::Objects;

CSound::CSound (Wallpapers::CScene& scene, const Sound& sound) :
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
}

void CSound::load () {
    for (const auto& cur : this->m_sound.sounds) {
        auto stream = new Audio::AudioStream (
            this->getScene ().getAudioContext (),
            this->getAssetLocator ().read (cur)
        );

        stream->setRepeat (this->m_sound.playbackmode.has_value() && this->m_sound.playbackmode == "loop");

        this->m_audioStreams.push_back (stream);

        // add the stream to the context so it can be played
        this->getScene ().getAudioContext ().addStream (stream);
    }
}

void CSound::render () {}
