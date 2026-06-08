#include "MediaSource.h"

#include <ranges>

using namespace WallpaperEngine::Media;

MediaSource::MediaSource (std::chrono::milliseconds updateInterval) :
    m_aliveFlag (std::make_unique<bool> (true)), m_mediaInfo (
						     {
							 .playbackState = PlaybackState::Stopped,
							 .title = "",
							 .artist = "",
							 .url = std::nullopt,
							 .duration = 0.0f,
							 .position = 0.0f,
							 .available = false,

						     }
						 ),
    m_updateInterval (updateInterval) { }

MediaSource::~MediaSource () {
    if (this->m_aliveFlag) {
	*m_aliveFlag = false;
    }

    m_albumArtListeners.clear ();
    m_metadataListeners.clear ();
}

void MediaSource::update () {
    if (std::chrono::steady_clock::now () <= m_nextUpdate) {
	return;
    }

    this->performUpdate ();
}

std::function<void ()> MediaSource::addMetadataListener (std::function<void (const MediaInfo&)> listener) {
    int listenerId = ++m_metadataListenerId;

    m_metadataListeners.emplace (listenerId, listener);

    auto alive = m_aliveFlag;

    return [this, listenerId, alive] () {
	if (!alive || !*alive) {
	    return;
	}

	m_metadataListeners.erase (listenerId);
    };
}

std::function<void ()> MediaSource::addAlbumArtListener (std::function<void (const MediaInfo&)> listener) {
    int listenerId = ++m_albumArtListenerId;

    m_albumArtListeners.emplace (listenerId, listener);

    auto alive = m_aliveFlag;

    return [this, listenerId, alive] () {
	if (!alive || !*alive) {
	    return;
	}

	m_albumArtListeners.erase (listenerId);
    };
}

void MediaSource::fireMetadataListeners () {
    for (auto& listener : m_metadataListeners | std::views::values) {
	listener (m_mediaInfo);
    }
}

void MediaSource::fireAlbumArtListeners () {
    for (auto& listener : m_albumArtListeners | std::views::values) {
	listener (m_mediaInfo);
    }
}