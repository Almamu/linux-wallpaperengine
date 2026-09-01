#include <catch2/catch_test_macros.hpp>

#include "WallpaperEngine/Audio/AudioPlaybackState.h"

using WallpaperEngine::Audio::AudioPlaybackState;

TEST_CASE ("Audio playback state preserves pause and rewinds after stop") {
    AudioPlaybackState state;

    CHECK (state.isPlaying ());

    state.pause ();
    CHECK_FALSE (state.isPlaying ());

    state.play ();
    CHECK (state.isPlaying ());

    const auto generation = state.stop ();
    CHECK_FALSE (state.isPlaying ());
    CHECK (state.resetPending ());

    state.play ();
    CHECK_FALSE (state.isPlaying ());

    state.markResetComplete (generation);
    CHECK_FALSE (state.resetPending ());
    CHECK (state.isPlaying ());
}

TEST_CASE ("Audio playback state ignores stale rewind completion and clamps volume") {
    AudioPlaybackState state;

    const auto staleGeneration = state.stop ();
    const auto currentGeneration = state.stop ();
    state.play ();

    state.markResetComplete (staleGeneration);
    CHECK (state.resetPending ());
    CHECK_FALSE (state.isPlaying ());

    state.markResetComplete (currentGeneration);
    CHECK (state.isPlaying ());

    state.setVolume (-0.5f);
    CHECK (state.volume () == 0.0f);
    state.setVolume (0.6f);
    CHECK (state.volume () == 0.6f);
    state.setVolume (1.5f);
    CHECK (state.volume () == 1.0f);
}
