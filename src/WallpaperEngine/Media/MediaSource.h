#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <string>

namespace WallpaperEngine::Media {
class MediaSource {
public:
    enum PlaybackState {
        Stopped = 0,
        Playing = 1,
        Paused = 2,
    };

    struct MediaInfo {
        PlaybackState playbackState;
        std::string title;
        std::string artist;
        std::string album;
        std::optional<std::string> url;
        double duration;
        double position;
        bool available;
    };

    explicit MediaSource (std::chrono::milliseconds updateInterval);
    virtual ~MediaSource () = default;

    virtual void update ();

    /**
     * @param listener The callback to call when media info changes
     *
     * @return A function that can be called to remove the listener
     */
    std::function<void()> addListener (std::function<void (MediaInfo&)> listener);

    [[nodiscard]] const MediaInfo& getMediaInfo () const { return m_mediaInfo; }

protected:
    void fireListeners ();
    virtual void performUpdate () = 0;

    uint32_t m_listenerId = 0;
    std::map<uint32_t, std::function<void (MediaInfo&)>> m_listeners;
    MediaInfo m_mediaInfo;
    std::chrono::milliseconds m_updateInterval;
    std::chrono::time_point<std::chrono::steady_clock> m_nextUpdate;
};
}