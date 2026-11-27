#include "PulseAudioPlayingDetector.h"
#include "WallpaperEngine/Logging/Log.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <unistd.h>

namespace WallpaperEngine::Audio::Drivers::Detectors {
namespace {
bool matchesIgnoreList (const pa_sink_input_info* info, const std::vector<std::string>& ignoreList) {
    const auto contains = [] (const char* haystack, const std::string& needle) {
	if (haystack == nullptr) {
	    return false;
	}
	std::string lowered = haystack;
	std::ranges::transform (lowered, lowered.begin (), [] (unsigned char c) { return std::tolower (c); });
	return lowered.find (needle) != std::string::npos;
    };

    const char* name = pa_proplist_gets (info->proplist, PA_PROP_APPLICATION_NAME);
    const char* binary = pa_proplist_gets (info->proplist, PA_PROP_APPLICATION_PROCESS_BINARY);

    return std::ranges::any_of (ignoreList, [&] (const std::string& entry) {
	std::string needle = entry;
	std::ranges::transform (needle, needle.begin (), [] (unsigned char c) { return std::tolower (c); });
	return contains (name, needle) || contains (binary, needle);
    });
}
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

    // Only count streams that are actually producing audio: corked means paused (an idle
    // browser tab, a paused player, or a bluetooth keep-alive stream sits corked/silent for
    // hours and would otherwise mute wallpaper audio permanently), and the mute flag is
    // separate from the volume level. Wallpaper Engine only mutes while something plays.
    // Wallpaper daemons are ignored wholesale: skwd-paper keeps an always-unmuted phantom
    // stream open and stale wallpaperengine instances leave their own — either would mute
    // us forever and automute could then never unmute (see --automute-ignore).
    if (value && strtol (value, nullptr, 10) != getpid () && !info->corked && !info->mute
	&& pa_cvolume_avg (&info->volume) != PA_VOLUME_MUTED
	&& !matchesIgnoreList (info, detector->getAutomuteIgnore ())) {
	detector->setIsPlaying (true);
    }
}

const std::vector<std::string>& PulseAudioPlayingDetector::getAutomuteIgnore () const {
    return this->getApplicationContext ().settings.audio.automuteIgnore;
}

void defaultSinkInfoCallback (pa_context* context, const pa_server_info* info, void* userdata) {
    if (info == nullptr) {
	return;
    }

    if (auto* op = pa_context_get_sink_input_info_list (context, sinkInputInfoCallback, userdata)) {
	pa_operation_unref (op);
    }
}

PulseAudioPlayingDetector::PulseAudioPlayingDetector (
    Application::ApplicationContext& appContext,
    const Render::Drivers::Detectors::FullScreenDetector& fullscreenDetector
) : AudioPlayingDetector (appContext, fullscreenDetector) {
    this->m_mainloop = pa_mainloop_new ();
    if (this->m_mainloop) {
	this->m_context = pa_context_new (pa_mainloop_get_api (this->m_mainloop), "wallpaperengine");
    }
    if (this->m_context && pa_context_connect (this->m_context, nullptr, PA_CONTEXT_NOFLAGS, nullptr) >= 0) {
	using Clock = std::chrono::steady_clock;
	const auto deadline = Clock::now () + std::chrono::seconds (1);
	while (true) {
	    const auto state = pa_context_get_state (this->m_context);
	    if (state == PA_CONTEXT_READY) {
		return;
	    }
	    const auto remaining = std::chrono::duration_cast<std::chrono::microseconds> (deadline - Clock::now ());
	    if (!PA_CONTEXT_IS_GOOD (state) || remaining.count () <= 0
		|| pa_mainloop_prepare (this->m_mainloop, static_cast<int> (remaining.count ())) < 0
		|| pa_mainloop_poll (this->m_mainloop) < 0 || pa_mainloop_dispatch (this->m_mainloop) < 0) {
		break;
	    }
	}
    }
    this->disconnect ();
    sLog.error ("PulseAudio connection unavailable. Audio playback detection is disabled");
}

PulseAudioPlayingDetector::~PulseAudioPlayingDetector () { this->disconnect (); }

void PulseAudioPlayingDetector::disconnect () {
    if (this->m_context) {
	pa_context_disconnect (this->m_context);
	pa_context_unref (this->m_context);
	this->m_context = nullptr;
    }

    if (this->m_mainloop) {
	pa_mainloop_free (this->m_mainloop);
	this->m_mainloop = nullptr;
    }
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
    if (!this->m_context || pa_context_get_state (this->m_context) != PA_CONTEXT_READY) {
	return;
    }

    // start discovery of sinks
    pa_operation* op = pa_context_get_server_info (this->m_context, defaultSinkInfoCallback, this);
    if (!op) {
	return;
    }

    while (pa_operation_get_state (op) == PA_OPERATION_RUNNING) {
	if (pa_context_get_state (this->m_context) != PA_CONTEXT_READY
	    || pa_mainloop_iterate (this->m_mainloop, 1, nullptr) < 0) {
	    pa_operation_cancel (op);
	    break;
	}
    }

    pa_operation_unref (op);
}
} // namespace WallpaperEngine::Audio::Drivers::Detectors
