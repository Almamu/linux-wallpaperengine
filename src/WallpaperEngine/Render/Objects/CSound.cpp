#include <SDL.h>
#include <algorithm>

#include "CSound.h"

#include "WallpaperEngine/FileSystem/Container.h"

using namespace WallpaperEngine::Render::Objects;

CSound::CSound (Wallpapers::CScene& scene, const Sound& sound) : CObject (scene, sound), m_sound (sound) {
    if (this->getContext ().getApp ().getContext ().settings.audio.enabled) {
	// Resolve effective playback mode: CLI > project JSON > default (sequential)
	const auto& audioSettings = this->getContext ().getApp ().getContext ().settings.audio;
	std::string effectiveMode = audioSettings.playbackMode;
	if (effectiveMode.empty ()) {
	    if (this->m_sound.playbackmode.has_value ()) {
		effectiveMode = this->m_sound.playbackmode.value ();
	    } else {
		effectiveMode = "sequential";
	    }
	}

	this->m_playlistLoop = (effectiveMode == "loop");
	this->m_shuffle = (effectiveMode == "shuffle");
	this->load ();
    }
}

CSound::~CSound () {
    // Remove active stream from audio context
    for (const auto& stream : this->m_activeStream) {
	this->getScene ().getAudioContext ().removeStream (stream.first);
    }
    this->m_activeStream.clear ();

    // Clean up any remaining streams (not yet played to completion)
    for (auto* stream : this->m_allStreams) {
	// Stop the stream first
	stream->stop ();
	// Wait for the audio thread to finish
	delete stream;
    }
    this->m_allStreams.clear ();
}

void CSound::load () {
    // Store track file names for on-demand stream creation
    this->m_trackFiles = this->m_sound.sounds;

    // Build play order (sequential or shuffled)
    this->m_playOrder.resize (this->m_trackFiles.size ());
    std::iota (this->m_playOrder.begin (), this->m_playOrder.end (), 0);
    if (this->m_shuffle) {
	std::shuffle (this->m_playOrder.begin (), this->m_playOrder.end (), this->m_rng);
    }

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

    // Track this stream for cleanup
    this->m_allStreams.push_back (stream);

    return stream;
}

void CSound::cleanupFinishedStream () {
    // Remove finished stream from active list and properly delete it
    for (const auto& stream : this->m_activeStream) {
	this->getScene ().getAudioContext ().removeStream (stream.first);

	// Find and delete the stream from m_allStreams
	auto it = std::find (this->m_allStreams.begin (), this->m_allStreams.end (), stream.second);
	if (it != this->m_allStreams.end ()) {
	    // Stop the stream first
	    stream.second->stop ();
	    delete stream.second;
	    this->m_allStreams.erase (it);
	}
    }
    this->m_activeStream.clear ();
}

void CSound::playNextTrack () {
    // Clean up finished stream
    this->cleanupFinishedStream ();

    // Check if we've reached the end of the playlist
    if (this->m_currentTrackIndex >= this->m_playOrder.size ()) {
	if (this->m_playlistLoop) {
	    this->m_currentTrackIndex = 0;
	    // Reshuffle if shuffle mode is enabled
	    if (this->m_shuffle) {
		std::shuffle (this->m_playOrder.begin (), this->m_playOrder.end (), this->m_rng);
	    }
	} else {
	    // Playlist finished, no more tracks to play
	    return;
	}
    }

    // Get the actual track index from play order
    const size_t actualTrackIndex = this->m_playOrder[this->m_currentTrackIndex];

    // Create a NEW stream for this track (allows replaying on loop)
    auto* stream = this->createStreamForTrack (actualTrackIndex);
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
