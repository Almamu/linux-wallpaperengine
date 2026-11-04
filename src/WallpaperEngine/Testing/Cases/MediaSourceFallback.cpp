#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <optional>
#include <string>

#include "WallpaperEngine/Media/DBusMediaSource.h"
#include "WallpaperEngine/Media/NullMediaSource.h"

using namespace WallpaperEngine::Media;

namespace {
/** Points libdbus at a session bus that doesn't exist, and puts the old address back afterwards */
class ScopedMissingSessionBus {
public:
    ScopedMissingSessionBus () {
	if (const char* old = std::getenv ("DBUS_SESSION_BUS_ADDRESS")) {
	    m_old = old;
	}

	setenv ("DBUS_SESSION_BUS_ADDRESS", "unix:path=/nonexistent/linux-wallpaperengine-test/bus", 1);
    }

    ~ScopedMissingSessionBus () {
	if (m_old) {
	    setenv ("DBUS_SESSION_BUS_ADDRESS", m_old->c_str (), 1);
	} else {
	    unsetenv ("DBUS_SESSION_BUS_ADDRESS");
	}
    }

private:
    std::optional<std::string> m_old;
};
} // namespace

TEST_CASE ("Media source: without a D-Bus session bus the application keeps running without media information") {
    ScopedMissingSessionBus missingBus;

    // used to throw out of WallpaperApplication's constructor and end the process (#725)
    std::unique_ptr<MediaSource> source;
    REQUIRE_NOTHROW (source = DBusMediaSource::createOrFallback (std::chrono::milliseconds (2000)));
    REQUIRE (source != nullptr);
    CHECK (dynamic_cast<NullMediaSource*> (source.get ()) != nullptr);

    // nothing is ever playing
    CHECK_FALSE (source->getMediaInfo ().available);
    CHECK (source->getMediaInfo ().playbackState == MediaSource::Stopped);
    REQUIRE_NOTHROW (source->update ());
    CHECK_FALSE (source->getMediaInfo ().available);
}

TEST_CASE ("Media source: the null source accepts listeners and never calls them") {
    NullMediaSource source (std::chrono::milliseconds (0));
    int calls = 0;

    const auto removeMetadata = source.addMetadataListener ([&calls] (const MediaSource::MediaInfo&) { calls++; });
    const auto removeAlbumArt = source.addAlbumArtListener ([&calls] (const MediaSource::MediaInfo&) { calls++; });

    for (int i = 0; i < 3; i++) {
	source.update ();
    }

    CHECK (calls == 0);

    removeMetadata ();
    removeAlbumArt ();
}
