#include "WaylandFullScreenDetector.h"

#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Render/Drivers/VideoFactories.h"
#include "wlr-foreign-toplevel-management-unstable-v1-protocol.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>
#include <wayland-client.h>

namespace WallpaperEngine::Render::Drivers::Detectors {

struct WaylandFullscreenToplevel {
    bool pending = false;
    bool current = false;
    bool pendingActivated = false;
    bool currentActivated = false;
    std::string appId {};
    /** Outputs the toplevel is on, as bound on the detector's connection */
    std::set<wl_output*> outputs {};
    WaylandFullscreenDetectorCallbackData* const data;
};

namespace {

    bool icontains (std::string_view haystack, std::string_view needle) {
	if (needle.empty ()) {
	    return true;
	}
	if (haystack.empty ()) {
	    return false;
	}

	auto toLower = [] (unsigned char c) {
	    if (c >= 'A' && c <= 'Z') {
		return static_cast<unsigned char> (c - 'A' + 'a');
	    }
	    return c;
	};

	for (size_t i = 0; i + needle.size () <= haystack.size (); ++i) {
	    bool match = true;
	    for (size_t j = 0; j < needle.size (); ++j) {
		if (toLower (static_cast<unsigned char> (haystack[i + j]))
		    != toLower (static_cast<unsigned char> (needle[j]))) {
		    match = false;
		    break;
		}
	    }
	    if (match) {
		return true;
	    }
	}

	return false;
    }

    void toplevelHandleTitle (void*, struct zwlr_foreign_toplevel_handle_v1*, const char*) { }

    void toplevelHandleAppId (void* data, struct zwlr_foreign_toplevel_handle_v1*, const char* appId) {
	const auto state = static_cast<WaylandFullscreenToplevel*> (data);
	if (appId) {
	    state->appId = appId;
	}
    }

    void toplevelHandleOutputEnter (void* data, struct zwlr_foreign_toplevel_handle_v1*, struct wl_output* output) {
	static_cast<WaylandFullscreenToplevel*> (data)->outputs.insert (output);
    }

    void toplevelHandleOutputLeave (void* data, struct zwlr_foreign_toplevel_handle_v1*, struct wl_output* output) {
	static_cast<WaylandFullscreenToplevel*> (data)->outputs.erase (output);
    }

    void
    toplevelHandleParent (void*, struct zwlr_foreign_toplevel_handle_v1*, struct zwlr_foreign_toplevel_handle_v1*) { }

    void toplevelHandleState (void* data, struct zwlr_foreign_toplevel_handle_v1*, struct wl_array* state) {
	const auto toplevel = static_cast<WaylandFullscreenToplevel*> (data);
	const auto begin = static_cast<uint32_t*> (state->data);

	toplevel->pending = false;
	toplevel->pendingActivated = false;

	for (auto it = begin; it < begin + state->size / sizeof (uint32_t); ++it) {
	    if (*it == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN) {
		toplevel->pending = true;
	    }
	    if (*it == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED) {
		toplevel->pendingActivated = true;
	    }
	}
    }

    bool isRelevant (
	const WallpaperEngine::Application::ApplicationContext& ctx, const bool fullscreen, const bool activated,
	const std::string& appId
    ) {
	if (!fullscreen) {
	    return false;
	}

	if (ctx.settings.render.pauseOnFullscreenOnlyWhenActive && !activated) {
	    return false;
	}

	if (!appId.empty ()) {
	    for (const auto& ignore : ctx.settings.render.fullscreenPauseIgnoreAppIds) {
		if (ignore.empty ()) {
		    continue;
		}
		if (icontains (appId, ignore)) {
		    return false;
		}
	    }
	}

	return true;
    }

    bool isFullscreenRelevant (const WaylandFullscreenToplevel& toplevel) {
	const auto& ctx = toplevel.data->detector->getApplicationContext ();
	return isRelevant (ctx, toplevel.pending, toplevel.pendingActivated, toplevel.appId);
    }

    bool isCurrentlyRelevant (const WaylandFullscreenToplevel& toplevel) {
	const auto& ctx = toplevel.data->detector->getApplicationContext ();
	return isRelevant (ctx, toplevel.current, toplevel.currentActivated, toplevel.appId);
    }

    void toplevelHandleDone (void* data, struct zwlr_foreign_toplevel_handle_v1* handle) {
	const auto toplevel = static_cast<WaylandFullscreenToplevel*> (data);

	const bool pendingRelevant = isFullscreenRelevant (*toplevel);
	const bool currentRelevant = isCurrentlyRelevant (*toplevel);

	if (currentRelevant != pendingRelevant) {
	    if (pendingRelevant) {
		++(*toplevel->data->fullscreenCount);
	    } else {
		if (*toplevel->data->fullscreenCount == 0) {
		    sLog.error ("Fullscreen count underflow!!!");
		} else {
		    --(*toplevel->data->fullscreenCount);
		}
	    }
	}

	toplevel->current = toplevel->pending;
	toplevel->currentActivated = toplevel->pendingActivated;
    }

    void toplevelHandleClosed (void* data, struct zwlr_foreign_toplevel_handle_v1* handle) {
	const auto toplevel = static_cast<WaylandFullscreenToplevel*> (data);

	// If it was counted as relevant fullscreen, remove it.
	const bool currentRelevant = isCurrentlyRelevant (*toplevel);

	if (currentRelevant) {
	    if (*toplevel->data->fullscreenCount == 0) {
		sLog.error ("Fullscreen count underflow!!!");
	    } else {
		--(*toplevel->data->fullscreenCount);
	    }
	}

	toplevel->data->toplevels->erase (toplevel);
	zwlr_foreign_toplevel_handle_v1_destroy (handle);
	delete toplevel;
    }

    constexpr struct zwlr_foreign_toplevel_handle_v1_listener toplevelHandleListener = {
	.title = toplevelHandleTitle,
	.app_id = toplevelHandleAppId,
	.output_enter = toplevelHandleOutputEnter,
	.output_leave = toplevelHandleOutputLeave,
	.state = toplevelHandleState,
	.done = toplevelHandleDone,
	.closed = toplevelHandleClosed,
	.parent = toplevelHandleParent,
    };

    void handleToplevel (
	void* data, struct zwlr_foreign_toplevel_manager_v1* manager, struct zwlr_foreign_toplevel_handle_v1* handle
    ) {
	const auto cb
	    = static_cast<WallpaperEngine::Render::Drivers::Detectors::WaylandFullscreenDetectorCallbackData*> (data);
	const auto toplevel = new WaylandFullscreenToplevel {
	    .pending = false,
	    .current = false,
	    .pendingActivated = false,
	    .currentActivated = false,
	    .appId = {},
	    .outputs = {},
	    .data = cb,
	};
	cb->toplevels->insert (toplevel);
	zwlr_foreign_toplevel_handle_v1_add_listener (handle, &toplevelHandleListener, toplevel);
    }

    void handleFinished (void* data, struct zwlr_foreign_toplevel_manager_v1* manager) {
	zwlr_foreign_toplevel_manager_v1_destroy (manager);
    }

    zwlr_foreign_toplevel_manager_v1_listener toplevelManagerListener = {
	.toplevel = handleToplevel,
	.finished = handleFinished,
    };

    void outputHandleGeometry (
	void*, struct wl_output*, int32_t, int32_t, int32_t, int32_t, int32_t, const char*, const char*, int32_t
    ) { }

    void outputHandleMode (void*, struct wl_output*, uint32_t, int32_t, int32_t, int32_t) { }

    void outputHandleDone (void*, struct wl_output*) { }

    void outputHandleScale (void*, struct wl_output*, int32_t) { }

    void outputHandleDescription (void*, struct wl_output*, const char*) { }

}; // anonymous namespace

void outputHandleName (void* data, struct wl_output* output, const char* name) {
    const auto detector = static_cast<WaylandFullScreenDetector*> (data);
    if (const auto it = detector->m_outputs.find (output); it != detector->m_outputs.end () && name) {
	it->second.name = name;
    }
}

constexpr struct wl_output_listener outputListener = {
    .geometry = outputHandleGeometry,
    .mode = outputHandleMode,
    .done = outputHandleDone,
    .scale = outputHandleScale,
    .name = outputHandleName,
    .description = outputHandleDescription,
};

void handleGlobal (void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
    const auto detector = static_cast<WaylandFullScreenDetector*> (data);
    if (strcmp (interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0) {
	// bound after the outputs so the compositor can tell which output each toplevel is on
	detector->m_toplevelManagerName = name;
    } else if (strcmp (interface, wl_output_interface.name) == 0) {
	// version 4 adds the output name, which is how outputs are matched with the viewports
	const auto output = static_cast<wl_output*> (
	    wl_registry_bind (registry, name, &wl_output_interface, std::min<uint32_t> (version, 4))
	);
	detector->m_outputs.emplace (output, WaylandFullscreenDetectorOutput { .globalName = name, .name = {} });
	wl_output_add_listener (output, &outputListener, detector);
    }
}

void handleGlobalRemoved (void* data, struct wl_registry* registry, uint32_t id) {
    const auto detector = static_cast<WaylandFullScreenDetector*> (data);
    for (auto it = detector->m_outputs.begin (); it != detector->m_outputs.end (); ++it) {
	if (it->second.globalName != id) {
	    continue;
	}

	for (const auto toplevel : detector->m_toplevels) {
	    toplevel->outputs.erase (it->first);
	}

	// release (version 3+) also frees the compositor's side, destroy only drops the proxy
	if (wl_output_get_version (it->first) >= WL_OUTPUT_RELEASE_SINCE_VERSION) {
	    wl_output_release (it->first);
	} else {
	    wl_output_destroy (it->first);
	}

	detector->m_outputs.erase (it);
	return;
    }
}

constexpr struct wl_registry_listener registryListener = {
    .global = handleGlobal,
    .global_remove = handleGlobalRemoved,
};

WaylandFullScreenDetector::WaylandFullScreenDetector (Application::ApplicationContext& appContext) :
    FullScreenDetector (appContext),
    m_callbackData { .detector = this, .fullscreenCount = &m_fullscreenCount, .toplevels = &m_toplevels } {
    m_display = wl_display_connect (nullptr);
    if (!m_display) {
	sLog.exception ("Failed to query wayland display");
    }

    const auto registry = wl_display_get_registry (m_display);
    wl_registry_add_listener (registry, &registryListener, this);
    wl_display_roundtrip (m_display); // load list of globals
    if (m_toplevelManagerName != 0) {
	m_toplevelManager = static_cast<zwlr_foreign_toplevel_manager_v1*> (
	    wl_registry_bind (registry, m_toplevelManagerName, &zwlr_foreign_toplevel_manager_v1_interface, 3)
	);
	if (m_toplevelManager) {
	    zwlr_foreign_toplevel_manager_v1_add_listener (
		m_toplevelManager, &toplevelManagerListener, &m_callbackData
	    );
	}
    }
    if (!m_toplevelManager) {
	sLog.out ("Fullscreen detection not supported by your Wayland compositor");
    } else {
	wl_display_roundtrip (m_display); // load toplevel details
    }
}

WaylandFullScreenDetector::~WaylandFullScreenDetector () {
    if (m_display) {
	wl_display_disconnect (m_display);
    }
}

bool WaylandFullScreenDetector::anythingFullscreen () const {
    if (!m_toplevelManager) {
	return false;
    }
    wl_display_roundtrip (m_display);
    return m_fullscreenCount > 0;
}

bool WaylandFullScreenDetector::canTellOutputsApart () const { return true; }

bool WaylandFullScreenDetector::isFullscreenOn (const std::string& output) const {
    if (!m_toplevelManager || m_fullscreenCount == 0) {
	return false;
    }

    for (const auto toplevel : m_toplevels) {
	if (!isCurrentlyRelevant (*toplevel)) {
	    continue;
	}

	// no idea where it is, so pause everything
	if (toplevel->outputs.empty ()) {
	    return true;
	}

	for (const auto handle : toplevel->outputs) {
	    const auto it = m_outputs.find (handle);
	    // without a name (wl_output older than version 4) there's no way to match it, so pause everything
	    if (it == m_outputs.end () || it->second.name.empty () || it->second.name == output) {
		return true;
	    }
	}
    }

    return false;
}

void WaylandFullScreenDetector::reset () { }

__attribute__ ((constructor)) void registerWaylandFullscreenDetector () {
    sVideoFactories.registerFullscreenDetector (
	"wayland", [] (ApplicationContext& context, VideoDriver& driver) -> std::unique_ptr<FullScreenDetector> {
	    return std::make_unique<WaylandFullScreenDetector> (context);
	}
    );
}

} // namespace WallpaperEngine::Render::Drivers::Detectors
