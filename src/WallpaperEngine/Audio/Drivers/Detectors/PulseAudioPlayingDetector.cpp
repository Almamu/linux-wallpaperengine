#include "PulseAudioPlayingDetector.h"
#include "WallpaperEngine/Logging/Log.h"

#include <unistd.h>

namespace WallpaperEngine::Audio::Drivers::Detectors {
namespace {
    // how long the server gets to accept a new connection
    constexpr std::chrono::milliseconds CONNECT_TIMEOUT { 2000 };
    // how long one query for playing streams may hold up a frame
    constexpr std::chrono::milliseconds QUERY_TIMEOUT { 100 };
    // how long to wait before trying to reach the server again
    constexpr std::chrono::seconds RECONNECT_INTERVAL { 5 };
    // upper bound of server events handled at the start of each frame
    constexpr int MAX_EVENTS_PER_FRAME = 32;
} // namespace

void sinkInputInfoCallback (pa_context* context, const pa_sink_input_info* info, int eol, void* userdata) {
    auto* detector = static_cast<PulseAudioPlayingDetector*> (userdata);

    if (info == nullptr) {
	return;
    }

    if (info->proplist == nullptr) {
	return;
    }

    // get processid
    const char* value = pa_proplist_gets (info->proplist, PA_PROP_APPLICATION_PROCESS_ID);

    if (value && strtol (value, nullptr, 10) != getpid () && pa_cvolume_avg (&info->volume) != PA_VOLUME_MUTED) {
	detector->setIsPlaying (true);
    }
}

PulseAudioPlayingDetector::PulseAudioPlayingDetector (
    Application::ApplicationContext& appContext,
    const Render::Drivers::Detectors::FullScreenDetector& fullscreenDetector
) : AudioPlayingDetector (appContext, fullscreenDetector) {
    this->m_mainloop = pa_mainloop_new ();
    this->m_mainloopApi = pa_mainloop_get_api (this->m_mainloop);

    this->connect ();
}

PulseAudioPlayingDetector::~PulseAudioPlayingDetector () {
    this->disconnect ();

    if (this->m_mainloop) {
	pa_mainloop_free (this->m_mainloop);
    }
}

bool PulseAudioPlayingDetector::isConnected () const {
    return this->m_context != nullptr && pa_context_get_state (this->m_context) == PA_CONTEXT_READY;
}

template <typename Predicate>
bool PulseAudioPlayingDetector::iterateUntil (Predicate done, const std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now () + timeout;

    while (!done ()) {
	const auto now = std::chrono::steady_clock::now ();

	if (now >= deadline) {
	    return false;
	}

	const auto remaining = std::chrono::duration_cast<std::chrono::microseconds> (deadline - now);

	// same steps as pa_mainloop_iterate, but never blocks past the deadline
	if (pa_mainloop_prepare (this->m_mainloop, static_cast<int> (remaining.count ())) < 0
	    || pa_mainloop_poll (this->m_mainloop) < 0 || pa_mainloop_dispatch (this->m_mainloop) < 0) {
	    return done ();
	}
    }

    return true;
}

void PulseAudioPlayingDetector::connect () {
    const auto now = std::chrono::steady_clock::now ();

    this->m_connectStarted = now;
    this->m_nextConnectAttempt = now + RECONNECT_INTERVAL;
    this->m_context = pa_context_new (this->m_mainloopApi, "wallpaperengine");

    if (this->m_context == nullptr || pa_context_connect (this->m_context, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
	this->connectionFailed ();
    }
}

void PulseAudioPlayingDetector::disconnect () {
    if (this->m_context == nullptr) {
	return;
    }

    pa_context_disconnect (this->m_context);
    pa_context_unref (this->m_context);
    this->m_context = nullptr;
}

void PulseAudioPlayingDetector::connectionFailed () {
    if (!this->m_unreachable) {
	sLog.error ("Cannot reach the PulseAudio server, detection of playing audio paused until it's back");
    }

    this->m_unreachable = true;
    this->disconnect ();
}

void PulseAudioPlayingDetector::update () {
    if (!this->getApplicationContext ().settings.audio.automute) {
	return this->setIsPlaying (false);
    }
    if (this->getFullscreenDetector ().anythingFullscreen ()) {
	return this->setIsPlaying (true);
    }

    // reset playing state
    this->setIsPlaying (false);

    // handle whatever the server sent since the last frame without waiting for more
    for (int event = 0; event < MAX_EVENTS_PER_FRAME && pa_mainloop_iterate (this->m_mainloop, 0, nullptr) > 0;
	 event++) { }

    // the server can go away at any time (e.g. when it's restarted), so keep reconnecting until it's back
    if (!this->isConnected ()) {
	if (this->m_context == nullptr) {
	    if (std::chrono::steady_clock::now () >= this->m_nextConnectAttempt) {
		this->connect ();
	    }

	    return;
	}

	const bool connecting = PA_CONTEXT_IS_GOOD (pa_context_get_state (this->m_context));

	// a server that doesn't finish the handshake in time is treated like a missing one
	if (!connecting || std::chrono::steady_clock::now () - this->m_connectStarted >= CONNECT_TIMEOUT) {
	    this->connectionFailed ();
	}

	return;
    }

    if (this->m_unreachable) {
	sLog.out ("Reconnected to the PulseAudio server, detection of playing audio resumed");
	this->m_unreachable = false;
    }

    // libpulse returns no operation when the connection has died since the last frame
    pa_operation* op = pa_context_get_sink_input_info_list (this->m_context, sinkInputInfoCallback, this);

    if (op == nullptr) {
	this->connectionFailed ();
	return;
    }

    // wait until the list is complete, the callback marks anything that's playing
    const bool done
	= this->iterateUntil ([op] { return pa_operation_get_state (op) != PA_OPERATION_RUNNING; }, QUERY_TIMEOUT);

    if (!done) {
	pa_operation_cancel (op);
    }

    pa_operation_unref (op);

    // a server that stops answering is dropped too, instead of holding up every frame
    if (!done) {
	this->connectionFailed ();
    }
}
} // namespace WallpaperEngine::Audio::Drivers::Detectors
