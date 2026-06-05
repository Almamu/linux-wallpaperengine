#include "MediaSource.h"

using namespace WallpaperEngine::Media;

MediaSource::MediaSource (std::chrono::milliseconds updateInterval)
    : m_mediaInfo ({
        .playbackState = PlaybackState::Stopped,
        .title = "",
        .artist = "",
        .url = "",
        .duration = 0.0f,
        .position = 0.0f,
        .available = false,

    }),
    m_updateInterval(updateInterval)
{
}

void MediaSource::update () {
    if (std::chrono::steady_clock::now () <= m_nextUpdate) {
        return;
    }

    this->performUpdate ();
}

std::function<void()> MediaSource::addListener (std::function<void (MediaInfo&)> listener) {
    int listenerId = ++m_listenerId;

    m_listeners.emplace(listenerId, listener);

    return [this, listenerId]() {
        m_listeners.erase(listenerId);
    };
}

void MediaSource::fireListeners () {
    for (auto& [id, listener] : m_listeners) {
        listener(m_mediaInfo);
    }
}