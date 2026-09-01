#include <SDL.h>

#include "CSound.h"

#include <algorithm>
#include <ranges>

#include "WallpaperEngine/FileSystem/Container.h"

using namespace WallpaperEngine::Render::Objects;

CSound::CSound (Wallpapers::CScene& scene, const Sound& sound) :
    CObject (scene, sound), Scripting::ScriptableObject (scene, sound), m_sound (sound) {
    if (this->getContext ().getApp ().getContext ().settings.audio.enabled) {
	this->load ();
    }
}

CSound::~CSound () {
    // free all the sound buffers and streams
    for (const auto& stream : this->m_audioStreams) {
	this->getScene ().getAudioContext ().removeStream (stream.first);
	delete stream.second;
    }

    this->m_audioStreams.clear ();
}

void CSound::load () {
    for (const auto& cur : this->m_sound.sounds) {
	auto stream
	    = new Audio::AudioStream (this->getScene ().getAudioContext (), this->getAssetLocator ().read (cur));

		stream->setRepeat (this->m_sound.playbackmode.has_value () && this->m_sound.playbackmode == "loop");
		stream->setVolume (this->m_volume);

	// add the stream to the context so it can be played
	this->m_audioStreams.insert_or_assign (this->getScene ().getAudioContext ().addStream (stream), stream);
    }
}

void CSound::render () { }

void CSound::play () {
    for (const auto& stream : this->m_audioStreams | std::views::values) {
		stream->play ();
    }
}

void CSound::pause () {
    for (const auto& stream : this->m_audioStreams | std::views::values) {
		stream->pause ();
    }
}

void CSound::stop () {
    for (const auto& stream : this->m_audioStreams | std::views::values) {
		stream->stopPlayback ();
    }
}

bool CSound::isPlaying () const {
    return std::ranges::any_of (this->m_audioStreams | std::views::values, [] (const auto* stream) {
		return stream->isPlaying ();
    });
}

float CSound::getVolume () const { return this->m_volume; }

void CSound::setVolume (float volume) {
    this->m_volume = std::clamp (volume, 0.0f, 1.0f);
    for (const auto& stream : this->m_audioStreams | std::views::values) {
		stream->setVolume (this->m_volume);
    }
}
