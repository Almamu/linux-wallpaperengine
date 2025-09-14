#pragma once

namespace WallpaperEngine::Audio::Drivers::Recorders {
class PlaybackRecorder {
  public:
    virtual ~PlaybackRecorder () = default;

    virtual void update ();

    float audio16 [16] = {0};
    float audio32 [32] = {0};
    float audio64 [64] = {0};
};
} // namespace WallpaperEngine::Audio::Drivers::Recorders