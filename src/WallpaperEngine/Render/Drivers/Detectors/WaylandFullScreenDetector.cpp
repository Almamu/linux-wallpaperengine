#include "WaylandFullScreenDetector.h"

#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Render/Drivers/VideoFactories.h"
#include "wlr-foreign-toplevel-management-unstable-v1-protocol.h"
#include <cstring>
#include <string>
#include <string_view>

namespace WallpaperEngine::Render::Drivers::Detectors {

namespace {

bool icontains (std::string_view haystack, std::string_view needle) {
    if (needle.empty ())
        return true;
    if (haystack.empty ())
        return false;

    auto toLower = [](unsigned char c) {
        if (c >= 'A' && c <= 'Z')
            return static_cast<unsigned char> (c - 'A' + 'a');
        return c;
    };

    for (size_t i = 0; i + needle.size () <= haystack.size (); ++i) {
        bool match = true;
        for (size_t j = 0; j < needle.size (); ++j) {
            if (toLower (static_cast<unsigned char> (haystack[i + j])) !=
                toLower (static_cast<unsigned char> (needle[j]))) {
                match = false;
                break;
            }
        }
        if (match)
            return true;
    }

    return false;
}

struct FullscreenState {
    bool pending;
    bool current;
    bool pendingActivated;
    bool currentActivated;
    std::string appId;
    WallpaperEngine::Render::Drivers::Detectors::WaylandFullscreenDetectorCallbackData* const data;
};

void toplevelHandleTitle (void*, struct zwlr_foreign_toplevel_handle_v1*, const char*) {}

void toplevelHandleAppId (void* data, struct zwlr_foreign_toplevel_handle_v1*, const char* appId) {
    const auto state = static_cast<FullscreenState*> (data);
    if (appId)
        state->appId = appId;
}

void toplevelHandleOutputEnter (void*, struct zwlr_foreign_toplevel_handle_v1*, struct wl_output*) {}

void toplevelHandleOutputLeave (void*, struct zwlr_foreign_toplevel_handle_v1*, struct wl_output*) {}

void toplevelHandleParent (void*, struct zwlr_foreign_toplevel_handle_v1*, struct zwlr_foreign_toplevel_handle_v1*) {}

void toplevelHandleState (void* data, struct zwlr_foreign_toplevel_handle_v1*, struct wl_array* state) {
    const auto toplevel = static_cast<FullscreenState*> (data);
    const auto begin = static_cast<uint32_t*> (state->data);

    toplevel->pending = false;
    toplevel->pendingActivated = false;

    for (auto it = begin; it < begin + state->size / sizeof (uint32_t); ++it) {
        if (*it == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN)
            toplevel->pending = true;
        if (*it == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED)
            toplevel->pendingActivated = true;
    }
}

bool isFullscreenRelevant (const FullscreenState& toplevel) {
    if (!toplevel.pending)
        return false;

    const auto& ctx = toplevel.data->detector->getApplicationContext ();

    if (ctx.settings.render.pauseOnFullscreenOnlyWhenActive && !toplevel.pendingActivated)
        return false;

    if (!toplevel.appId.empty ()) {
        for (const auto& ignore : ctx.settings.render.fullscreenPauseIgnoreAppIds) {
            if (icontains (toplevel.appId, ignore))
                return false;
        }
    }

    return true;
}

bool isCurrentlyRelevant (const FullscreenState& toplevel) {
    if (!toplevel.current)
        return false;

    const auto& ctx = toplevel.data->detector->getApplicationContext ();

    if (ctx.settings.render.pauseOnFullscreenOnlyWhenActive && !toplevel.currentActivated)
        return false;

    if (!toplevel.appId.empty ()) {
        for (const auto& ignore : ctx.settings.render.fullscreenPauseIgnoreAppIds) {
            if (icontains (toplevel.appId, ignore))
                return false;
        }
    }

    return true;
}

void toplevelHandleDone (void* data, struct zwlr_foreign_toplevel_handle_v1* handle) {
    const auto toplevel = static_cast<FullscreenState*> (data);

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
    const auto toplevel = static_cast<FullscreenState*> (data);

    // If it was counted as relevant fullscreen, remove it.
    const bool currentRelevant = isCurrentlyRelevant (*toplevel);

    if (currentRelevant) {
        if (*toplevel->data->fullscreenCount == 0) {
            sLog.error ("Fullscreen count underflow!!!");
        } else {
            --(*toplevel->data->fullscreenCount);
        }
    }

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

void handleToplevel (void* data, struct zwlr_foreign_toplevel_manager_v1* manager,
                     struct zwlr_foreign_toplevel_handle_v1* handle) {
    const auto cb = static_cast<WallpaperEngine::Render::Drivers::Detectors::WaylandFullscreenDetectorCallbackData*> (data);
    const auto toplevel = new FullscreenState {.data = cb};
    zwlr_foreign_toplevel_handle_v1_add_listener (handle, &toplevelHandleListener, toplevel);
}

void handleFinished (void* data, struct zwlr_foreign_toplevel_manager_v1* manager) {
    zwlr_foreign_toplevel_manager_v1_destroy (manager);
}

zwlr_foreign_toplevel_manager_v1_listener toplevelManagerListener = {
    .toplevel = handleToplevel,
    .finished = handleFinished,
};

}; // anonymous namespace

void handleGlobal (void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
    const auto detector = static_cast<WaylandFullScreenDetector*> (data);
    if (strcmp (interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0) {
        detector->m_toplevelManager = static_cast<zwlr_foreign_toplevel_manager_v1*> (
            wl_registry_bind (registry, name, &zwlr_foreign_toplevel_manager_v1_interface, 3));
        if (detector->m_toplevelManager) {
            zwlr_foreign_toplevel_manager_v1_add_listener (detector->m_toplevelManager, &toplevelManagerListener,
                                                           &detector->m_callbackData);
        }
    }
}

void handleGlobalRemoved (void* data, struct wl_registry* registry, uint32_t id) {
    // todo: outputs
}

constexpr struct wl_registry_listener registryListener = {
    .global = handleGlobal,
    .global_remove = handleGlobalRemoved,
};

WaylandFullScreenDetector::WaylandFullScreenDetector (Application::ApplicationContext& appContext) :
    FullScreenDetector (appContext),
    m_callbackData {.detector = this, .fullscreenCount = &m_fullscreenCount} {
    m_display = wl_display_connect (nullptr);
    if (!m_display)
        sLog.exception ("Failed to query wayland display");

    const auto registry = wl_display_get_registry (m_display);
    wl_registry_add_listener (registry, &registryListener, this);
    wl_display_roundtrip (m_display); // load list of toplevels
    if (!m_toplevelManager) {
        sLog.out ("Fullscreen detection not supported by your Wayland compositor");
    } else {
        wl_display_roundtrip (m_display); // load toplevel details
    }
}

WaylandFullScreenDetector::~WaylandFullScreenDetector () {
    if (m_display)
        wl_display_disconnect (m_display);
}

bool WaylandFullScreenDetector::anythingFullscreen () const {
    if (!m_toplevelManager) {
        return false;
    }
    wl_display_roundtrip (m_display);
    return m_fullscreenCount > 0;
}

void WaylandFullScreenDetector::reset () {}


__attribute__((constructor)) void registerWaylandFullscreenDetector () {
    sVideoFactories.registerFullscreenDetector(
        "wayland",
        [](ApplicationContext& context, VideoDriver& driver) -> std::unique_ptr<FullScreenDetector> {
            return std::make_unique <WaylandFullScreenDetector> (context);
        }
    );
}

} // namespace WallpaperEngine::Render::Drivers::Detectors
