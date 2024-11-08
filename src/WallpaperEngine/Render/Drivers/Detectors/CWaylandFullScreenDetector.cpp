#include "CWaylandFullScreenDetector.h"

#include "WallpaperEngine/Logging/CLog.h"
#include "wlr-foreign-toplevel-management-unstable-v1-protocol.h"
#include <cstring>

namespace WallpaperEngine::Render::Drivers::Detectors {

namespace {

struct FullscreenState {
    bool pending;
    bool current;
    uint32_t* const count;
};

void toplevelHandleTitle (void*, struct zwlr_foreign_toplevel_handle_v1*, const char*) {}

void toplevelHandleAppId (void*, struct zwlr_foreign_toplevel_handle_v1*, const char*) {}

void toplevelHandleOutputEnter (void*, struct zwlr_foreign_toplevel_handle_v1*, struct wl_output*) {}

void toplevelHandleOutputLeave (void*, struct zwlr_foreign_toplevel_handle_v1*, struct wl_output*) {}

void toplevelHandleParent (void*, struct zwlr_foreign_toplevel_handle_v1*, struct zwlr_foreign_toplevel_handle_v1*) {}

void toplevelHandleState (void* data, struct zwlr_foreign_toplevel_handle_v1* handle, struct wl_array* state) {
    auto fullscreen = static_cast<FullscreenState*> (data);
    const auto begin = static_cast<uint32_t*> (state->data);

    fullscreen->pending = false;
    for (auto it = begin; it < begin + state->size / sizeof (uint32_t); ++it)
        if (*it == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN)
            fullscreen->pending = true;
}

void toplevelHandleDone (void* data, struct zwlr_foreign_toplevel_handle_v1* handle) {
    auto fullscreen = static_cast<FullscreenState*> (data);
    if (fullscreen->current != fullscreen->pending) {
        fullscreen->current = fullscreen->pending;
        if (fullscreen->current) {
            ++(*fullscreen->count);
        } else {
            // sanity check
            if (*fullscreen->count == 0) {
                sLog.error ("Fullscreen count underflow!!!");
            } else {
                --(*fullscreen->count);
            }
        }
    }
}

void toplevelHandleClosed (void* data, struct zwlr_foreign_toplevel_handle_v1* handle) {
    auto fullscreen = static_cast<FullscreenState*> (data);

    if (fullscreen->current) {
        // sanity check
        if (*fullscreen->count == 0) {
            sLog.error ("Fullscreen count underflow!!!");
        } else {
            --(*fullscreen->count);
        }
    }

    zwlr_foreign_toplevel_handle_v1_destroy (handle);
    delete fullscreen;
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
    auto fullscreen = new FullscreenState {.count = static_cast<uint32_t*> (data)};
    zwlr_foreign_toplevel_handle_v1_add_listener (handle, &toplevelHandleListener, fullscreen);
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
    const auto detector = static_cast<CWaylandFullScreenDetector*> (data);
    if (strcmp (interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0) {
        detector->m_toplevelManager = static_cast<zwlr_foreign_toplevel_manager_v1*> (
            wl_registry_bind (registry, name, &zwlr_foreign_toplevel_manager_v1_interface, 3));
        if (detector->m_toplevelManager) {
            zwlr_foreign_toplevel_manager_v1_add_listener (detector->m_toplevelManager, &toplevelManagerListener,
                                                           &detector->m_fullscreenCount);
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

CWaylandFullScreenDetector::CWaylandFullScreenDetector (Application::CApplicationContext& appContext) :
    CFullScreenDetector (appContext) {
    m_display = wl_display_connect (nullptr);
    if (!m_display)
        sLog.exception ("Failed to query wayland display");

    auto registry = wl_display_get_registry (m_display);
    wl_registry_add_listener (registry, &registryListener, this);
    wl_display_roundtrip (m_display); // load list of toplevels
    if (!m_toplevelManager) {
        sLog.out ("Fullscreen detection not supported by your Wayland compositor");
    } else {
        wl_display_roundtrip (m_display); // load toplevel details
    }
}

CWaylandFullScreenDetector::~CWaylandFullScreenDetector () {
    if (m_display)
        wl_display_disconnect (m_display);
}

bool CWaylandFullScreenDetector::anythingFullscreen () const {
    if (!m_toplevelManager) {
        return false;
    }
    wl_display_roundtrip (m_display);
    return m_fullscreenCount > 0;
}

void CWaylandFullScreenDetector::reset () {}

} // namespace WallpaperEngine::Render::Drivers::Detectors
