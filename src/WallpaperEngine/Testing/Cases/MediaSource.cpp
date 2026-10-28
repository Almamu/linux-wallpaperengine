#include <catch2/catch_test_macros.hpp>

#include "WallpaperEngine/Media/MediaSource.h"

namespace {
class CountingSource : public WallpaperEngine::Media::MediaSource {
public:
    CountingSource () : MediaSource (std::chrono::hours (1)) { }

    int calls = 0;

    void expireInterval () { m_nextUpdate = std::chrono::steady_clock::now () - std::chrono::seconds (1); }

protected:
    void performUpdate () override { ++calls; }
};
}

TEST_CASE ("Media sources poll once per interval and resume after it expires") {
    CountingSource source;
    source.update ();
    REQUIRE (source.calls == 1);

    for (int frame = 0; frame < 1000; ++frame) {
	source.update ();
    }
    CHECK (source.calls == 1);

    source.expireInterval ();
    source.update ();
    CHECK (source.calls == 2);
    source.update ();
    CHECK (source.calls == 2);
}
