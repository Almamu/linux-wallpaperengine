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

    this->m_activeStream.clear ();
}

void CSound::load () {
    // Store track file names for on-demand stream creation
    this->m_trackFiles = this->m_sound.sounds;

    // Start playing the first track
    if (!this->m_trackFiles.empty ()) {
	this->playNextTrack ();
    }
}

Audio::AudioStream* CSound::createStreamForTrack (size_t trackIndex) {
    if (trackIndex >= this->m_trackFiles.size ()) {
	return nullptr;
    }

    auto stream = new Audio::AudioStream (
	this->getScene ().getAudioContext (), this->getAssetLocator ().read (this->m_trackFiles[trackIndex]));

    // Individual tracks don't loop; the playlist loops as a whole if playbackmode is "loop"
    stream->setRepeat (false);
    return stream;
}

void CSound::playNextTrack () {
    // Remove current active stream if any
    for (const auto& stream : this->m_activeStream) {
	this->getScene ().getAudioContext ().removeStream (stream.first);
    }
    this->m_activeStream.clear ();

    // Check if we've reached the end of the playlist
    if (this->m_currentTrackIndex >= this->m_trackFiles.size ()) {
	if (this->m_playlistLoop) {
	    this->m_currentTrackIndex = 0;
	} else {
	    // Playlist finished, no more tracks to play
	    return;
	}
    }

    // Create a NEW stream for this track (allows replaying on loop)
    auto* stream = this->createStreamForTrack (this->m_currentTrackIndex);
    if (!stream) {
	return;
    }

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
