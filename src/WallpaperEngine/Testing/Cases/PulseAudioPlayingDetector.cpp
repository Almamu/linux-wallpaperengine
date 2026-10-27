#include <catch2/catch_test_macros.hpp>

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Audio/Drivers/Detectors/PulseAudioPlayingDetector.h"
#include "WallpaperEngine/Render/Drivers/Detectors/FullScreenDetector.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

using namespace WallpaperEngine;
using Application::ApplicationContext;
using Audio::Drivers::Detectors::PulseAudioPlayingDetector;
using Render::Drivers::Detectors::FullScreenDetector;

namespace {
const char* detectorArgv[] = { "linux-wallpaperengine" };

std::filesystem::path makeTempDir () {
    char path[] = "/tmp/lwe-pulse-test-XXXXXX";

    REQUIRE (mkdtemp (path) != nullptr);

    return path;
}

/**
 * Points libpulse at the given server while in scope, restoring whatever was set before
 */
class ScopedPulseServer {
public:
    explicit ScopedPulseServer (const std::string& server) {
	if (const char* previous = std::getenv ("PULSE_SERVER")) {
	    m_previous = previous;
	    m_hadPrevious = true;
	}

	setenv ("PULSE_SERVER", server.c_str (), 1);
    }

    ~ScopedPulseServer () {
	if (m_hadPrevious) {
	    setenv ("PULSE_SERVER", m_previous.c_str (), 1);
	} else {
	    unsetenv ("PULSE_SERVER");
	}
    }

private:
    std::string m_previous;
    bool m_hadPrevious = false;
};

/**
 * A unix socket that takes connections but never answers, like a server that's stuck
 */
class SilentServer {
public:
    SilentServer () : m_dir (makeTempDir ()) {
	const std::string path = (m_dir / "native").string ();
	sockaddr_un address {};

	address.sun_family = AF_UNIX;
	strncpy (address.sun_path, path.c_str (), sizeof (address.sun_path) - 1);

	m_fd = socket (AF_UNIX, SOCK_STREAM, 0);

	REQUIRE (m_fd >= 0);
	REQUIRE (bind (m_fd, reinterpret_cast<sockaddr*> (&address), sizeof (address)) == 0);
	REQUIRE (listen (m_fd, 8) == 0);

	m_address = "unix:" + path;
    }

    ~SilentServer () {
	close (m_fd);
	std::filesystem::remove_all (m_dir);
    }

    [[nodiscard]] const std::string& address () const { return m_address; }

private:
    std::filesystem::path m_dir;
    std::string m_address;
    int m_fd = -1;
};

class AlwaysFullscreen final : public FullScreenDetector {
public:
    using FullScreenDetector::FullScreenDetector;

    [[nodiscard]] bool anythingFullscreen () const override { return true; }
};

template <typename Function> std::chrono::milliseconds timed (Function function) {
    const auto start = std::chrono::steady_clock::now ();

    function ();

    return std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::steady_clock::now () - start);
}

/**
 * Updates the detector like the render loop does for the given time
 *
 * @return The longest a single update took
 */
std::chrono::milliseconds runFrames (PulseAudioPlayingDetector& detector, const std::chrono::milliseconds duration) {
    const auto end = std::chrono::steady_clock::now () + duration;
    std::chrono::milliseconds worst { 0 };

    while (std::chrono::steady_clock::now () < end) {
	worst = std::max (worst, timed ([&] { detector.update (); }));
	std::this_thread::sleep_for (std::chrono::milliseconds (10));
    }

    return worst;
}
} // namespace

TEST_CASE ("PulseAudioPlayingDetector keeps going when no server is running") {
    const auto dir = makeTempDir ();
    ScopedPulseServer server ("unix:" + (dir / "missing").string ());
    ApplicationContext context (1, const_cast<char**> (detectorArgv));
    FullScreenDetector fullscreen (context);

    // this used to wait forever for the connection to become ready
    std::unique_ptr<PulseAudioPlayingDetector> detector;
    const auto constructTime
	= timed ([&] { detector = std::make_unique<PulseAudioPlayingDetector> (context, fullscreen); });

    CHECK (constructTime < std::chrono::milliseconds (100));

    // this used to abort in libpulse, as the dead connection hands back no operation
    const auto worstUpdate = runFrames (*detector, std::chrono::milliseconds (1500));

    CHECK (worstUpdate < std::chrono::milliseconds (50));
    CHECK_FALSE (detector->isConnected ());
    CHECK_FALSE (detector->anythingPlaying ());

    detector.reset ();
    std::filesystem::remove_all (dir);
}

TEST_CASE ("PulseAudioPlayingDetector doesn't hang on a server that never answers") {
    SilentServer silent;
    ScopedPulseServer server (silent.address ());
    ApplicationContext context (1, const_cast<char**> (detectorArgv));
    FullScreenDetector fullscreen (context);

    std::unique_ptr<PulseAudioPlayingDetector> detector;
    const auto constructTime
	= timed ([&] { detector = std::make_unique<PulseAudioPlayingDetector> (context, fullscreen); });

    CHECK (constructTime < std::chrono::milliseconds (100));

    // long enough for the connection attempt to time out, without any frame waiting on it
    const auto worstUpdate = runFrames (*detector, std::chrono::milliseconds (3000));

    CHECK (worstUpdate < std::chrono::milliseconds (50));
    CHECK_FALSE (detector->isConnected ());
    CHECK_FALSE (detector->anythingPlaying ());
}

TEST_CASE ("PulseAudioPlayingDetector settings still apply without a server") {
    const auto dir = makeTempDir ();
    ScopedPulseServer server ("unix:" + (dir / "missing").string ());
    ApplicationContext context (1, const_cast<char**> (detectorArgv));

    SECTION ("anything fullscreen counts as playing") {
	AlwaysFullscreen fullscreen (context);
	PulseAudioPlayingDetector detector (context, fullscreen);

	detector.update ();

	CHECK (detector.anythingPlaying ());
    }

    SECTION ("automute turned off never counts as playing") {
	AlwaysFullscreen fullscreen (context);
	PulseAudioPlayingDetector detector (context, fullscreen);

	context.settings.audio.automute = false;
	detector.update ();

	CHECK_FALSE (detector.anythingPlaying ());
    }

    std::filesystem::remove_all (dir);
}
