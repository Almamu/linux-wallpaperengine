#include <SDL.h>

#include "CSound.h"

#include "WallpaperEngine/FileSystem/Container.h"

using namespace WallpaperEngine::Render::Objects;

CSound::CSound (Wallpapers::CScene& scene, const Sound& sound) : CObject (scene, sound), m_sound (sound) {
    if (this->getContext ().getApp ().getContext ().settings.audio.enabled) {
	this->m_playlistLoop = this->m_sound.playbackmode.has_value () && this->m_sound.playbackmode == "loop";
	this->load ();
    }
}

CSound::~CSound () {
    // Remove active stream from audio context
    for (const auto& stream : this->m_activeStream) {
	this->getScene ().getAudioContext ().removeStream (stream.first);
    }

    // free all the sound buffers and streams
    for (auto* stream : this->m_allStreams) {
	delete stream;
    }

    this->m_allStreams.clear ();
    this->m_activeStream.clear ();
}

void CSound::load () {
    for (const auto& cur : this->m_sound.sounds) {
	auto stream
	    = new Audio::AudioStream (this->getScene ().getAudioContext (), this->getAssetLocator ().read (cur));

	// Individual tracks don't loop; the playlist loops as a whole if playbackmode is "loop"
	stream->setRepeat (false);

	this->m_allStreams.push_back (stream);
    }

    // Start playing the first track
    if (!this->m_allStreams.empty ()) {
	this->playNextTrack ();
    }
}

void CSound::playNextTrack () {
    // Remove current active stream if any
    for (const auto& stream : this->m_activeStream) {
	this->getScene ().getAudioContext ().removeStream (stream.first);
    }
    this->m_activeStream.clear ();

    // Check if we've reached the end of the playlist
    if (this->m_currentTrackIndex >= this->m_allStreams.size ()) {
	if (this->m_playlistLoop) {
	    this->m_currentTrackIndex = 0;
	} else {
	    // Playlist finished, no more tracks to play
	    return;
	}
    }

    // Add the next track to the audio context
    auto* stream = this->m_allStreams[this->m_currentTrackIndex];
    int streamId = this->getScene ().getAudioContext ().addStream (stream);
    this->m_activeStream.insert_or_assign (streamId, stream);
    this->m_currentTrackIndex++;
}

void CSound::render () {
    // Check if the current track has finished playing
    for (const auto& stream : this->m_activeStream) {
	if (stream.second->isFinished ()) {
	    this->playNextTrack ();
	    break;
	}
    }
}
