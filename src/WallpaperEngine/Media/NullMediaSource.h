#pragma once

#include "MediaSource.h"

namespace WallpaperEngine::Media {
/**
 * A media source that never has anything playing. Used when the real one can't start (e.g. there is no D-Bus
 * session bus), so wallpapers still run, just without media information.
 */
class NullMediaSource final : public MediaSource {
public:
    explicit NullMediaSource (std::chrono::milliseconds updateInterval) : MediaSource (updateInterval) { }

protected:
    void performUpdate () override { }
};
} // namespace WallpaperEngine::Media
