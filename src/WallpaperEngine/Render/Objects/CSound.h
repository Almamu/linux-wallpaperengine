#pragma once

#include "WallpaperEngine/Audio/AudioStream.h"
#include "WallpaperEngine/Render/CObject.h"

#include <random>

using namespace WallpaperEngine;

namespace WallpaperEngine::Render::Wallpapers {
class CScene;
}

namespace WallpaperEngine::Render::Objects {
using namespace WallpaperEngine::Data::Model;

class CSound final : public CObject {
public:
    CSound (Wallpapers::CScene& scene, const Sound& sound);
    ~CSound () override;

    void render () override;

protected:
    void load ();

private:
    void playNextTrack ();
    Audio::AudioStream* createStreamForTrack (size_t trackIndex);
    void cleanupFinishedStream ();

    std::vector<std::string> m_trackFiles {};
    std::vector<size_t> m_playOrder {};  // Shuffled play order
    std::vector<Audio::AudioStream*> m_allStreams {};  // All created streams for cleanup
    std::map<int, Audio::AudioStream*> m_activeStream {};  // Only one stream active at a time
    size_t m_currentTrackIndex = 0;
    bool m_playlistLoop = false;
    bool m_shuffle = false;
    std::mt19937 m_rng { std::random_device {}() };

    const Sound& m_sound;
};
} // namespace WallpaperEngine::Render::Objects
