#include <catch2/catch_test_macros.hpp>

#include "WallpaperEngine/Media/MediaSource.h"

#include <chrono>
#include <functional>
#include <stdexcept>
#include <thread>

using namespace std::chrono_literals;
using WallpaperEngine::Media::MediaSource;

namespace {
/**
 * Media source that counts how often the base class asks it to poll
 */
class CountingMediaSource final : public MediaSource {
public:
    explicit CountingMediaSource (std::chrono::milliseconds interval, std::function<void ()> onUpdate = nullptr) :
	MediaSource (interval), m_onUpdate (std::move (onUpdate)) { }

    int polls = 0;

protected:
    void performUpdate () override {
	++polls;

	if (m_onUpdate) {
	    m_onUpdate ();
	}
    }

private:
    std::function<void ()> m_onUpdate;
};
} // namespace

TEST_CASE ("MediaSource polls once per update interval") {
    CountingMediaSource source (10s);

    // the render loop calls update () every frame
    for (int frame = 0; frame < 100; frame++) {
	source.update ();
    }

    REQUIRE (source.polls == 1);
}

TEST_CASE ("MediaSource polls again once the interval has passed") {
    CountingMediaSource source (20ms);

    source.update ();
    std::this_thread::sleep_for (60ms);
    source.update ();

    REQUIRE (source.polls == 2);
}

TEST_CASE ("MediaSource waits a full interval after a poll that throws") {
    CountingMediaSource source (10s, [] { throw std::runtime_error ("player went away"); });

    REQUIRE_THROWS (source.update ());

    for (int frame = 0; frame < 10; frame++) {
	source.update ();
    }

    REQUIRE (source.polls == 1);
}

TEST_CASE ("MediaSource counts the interval from the end of a slow poll") {
    // a poll that blocks for longer than the interval must not make the next frame poll again;
    // the interval is wide so the second update () lands inside it even on a busy CI runner
    CountingMediaSource source (500ms, [] { std::this_thread::sleep_for (600ms); });

    source.update ();
    source.update ();

    REQUIRE (source.polls == 1);
}
