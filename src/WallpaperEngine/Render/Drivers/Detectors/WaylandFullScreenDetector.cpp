#include "WaylandFullScreenDetector.h"

#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Render/Drivers/VideoFactories.h"
#include "wlr-foreign-toplevel-management-unstable-v1-protocol.h"
#include <cstring>
#include <dbus/dbus.h>
#include <fstream>
#include <string>
#include <string_view>
#include <unistd.h>

namespace WallpaperEngine::Render::Drivers::Detectors {

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

    struct FullscreenState {
	bool pending = false;
	bool current = false;
	bool pendingMaximized = false;
	bool currentMaximized = false;
	bool pendingActivated = false;
	bool currentActivated = false;
	std::string appId {};
	WallpaperEngine::Render::Drivers::Detectors::WaylandFullscreenDetectorCallbackData* const data;
    };

    void toplevelHandleTitle (void*, struct zwlr_foreign_toplevel_handle_v1*, const char*) { }

    void toplevelHandleAppId (void* data, struct zwlr_foreign_toplevel_handle_v1*, const char* appId) {
	const auto state = static_cast<FullscreenState*> (data);
	if (appId) {
	    state->appId = appId;
	}
    }

    void toplevelHandleOutputEnter (void*, struct zwlr_foreign_toplevel_handle_v1*, struct wl_output*) { }

    void toplevelHandleOutputLeave (void*, struct zwlr_foreign_toplevel_handle_v1*, struct wl_output*) { }

    void
    toplevelHandleParent (void*, struct zwlr_foreign_toplevel_handle_v1*, struct zwlr_foreign_toplevel_handle_v1*) { }

    void toplevelHandleState (void* data, struct zwlr_foreign_toplevel_handle_v1*, struct wl_array* state) {
	const auto toplevel = static_cast<FullscreenState*> (data);
	const auto begin = static_cast<uint32_t*> (state->data);

	toplevel->pending = false;
	toplevel->pendingMaximized = false;
	toplevel->pendingActivated = false;

	for (auto it = begin; it < begin + state->size / sizeof (uint32_t); ++it) {
	    if (*it == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED) {
		toplevel->pendingMaximized = true;
	    }
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

    bool isStateRelevant (
	const FullscreenState& toplevel, const bool isFullscreen, const bool isMaximized, const bool isActivated
    ) {
	const auto& ctx = toplevel.data->detector->getApplicationContext ();
	return isRelevant (ctx, isFullscreen || isMaximized, isActivated, toplevel.appId);
    }

    bool isPendingRelevant (const FullscreenState& toplevel) {
	return isStateRelevant (toplevel, toplevel.pending, toplevel.pendingMaximized, toplevel.pendingActivated);
    }

    bool isCurrentRelevant (const FullscreenState& toplevel) {
	return isStateRelevant (toplevel, toplevel.current, toplevel.currentMaximized, toplevel.currentActivated);
    }

    void toplevelHandleDone (void* data, struct zwlr_foreign_toplevel_handle_v1* handle) {
	const auto toplevel = static_cast<FullscreenState*> (data);

	const bool pendingRelevant = isPendingRelevant (*toplevel);
	const bool currentRelevant = isCurrentRelevant (*toplevel);

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
	toplevel->currentMaximized = toplevel->pendingMaximized;
	toplevel->currentActivated = toplevel->pendingActivated;
    }

    void toplevelHandleClosed (void* data, struct zwlr_foreign_toplevel_handle_v1* handle) {
	const auto toplevel = static_cast<FullscreenState*> (data);

	// If it was counted as relevant fullscreen, remove it.
	const bool currentRelevant = isCurrentRelevant (*toplevel);

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

    void handleToplevel (
	void* data, struct zwlr_foreign_toplevel_manager_v1* manager, struct zwlr_foreign_toplevel_handle_v1* handle
    ) {
	const auto cb
	    = static_cast<WallpaperEngine::Render::Drivers::Detectors::WaylandFullscreenDetectorCallbackData*> (data);
	const auto toplevel = new FullscreenState {
	    .pending = false,
	    .current = false,
	    .pendingMaximized = false,
	    .currentMaximized = false,
	    .pendingActivated = false,
	    .currentActivated = false,
	    .appId = {},
	    .data = cb,
	};
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
	    wl_registry_bind (registry, name, &zwlr_foreign_toplevel_manager_v1_interface, 3)
	);
	if (detector->m_toplevelManager) {
	    zwlr_foreign_toplevel_manager_v1_add_listener (
		detector->m_toplevelManager, &toplevelManagerListener, &detector->m_callbackData
	    );
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

void WaylandFullScreenDetector::initKWinDetector () {
    DBusError err;
    dbus_error_init (&err);

    this->m_dbusConnection = dbus_bus_get (DBUS_BUS_SESSION, &err);
    if (!this->m_dbusConnection) {
	dbus_error_free (&err);
	return;
    }

    // Check if KWin is running on the session bus
    DBusMessage* hasOwnerMsg = dbus_message_new_method_call (
	"org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameHasOwner"
    );
    if (!hasOwnerMsg) {
	dbus_connection_unref (this->m_dbusConnection);
	this->m_dbusConnection = nullptr;
	return;
    }

    const char* kwinService = "org.kde.KWin";
    dbus_message_append_args (hasOwnerMsg, DBUS_TYPE_STRING, &kwinService, DBUS_TYPE_INVALID);
    DBusMessage* hasOwnerReply
	= dbus_connection_send_with_reply_and_block (this->m_dbusConnection, hasOwnerMsg, 1000, &err);
    dbus_message_unref (hasOwnerMsg);

    dbus_bool_t hasOwner = FALSE;
    if (hasOwnerReply) {
	dbus_message_get_args (hasOwnerReply, &err, DBUS_TYPE_BOOLEAN, &hasOwner, DBUS_TYPE_INVALID);
	dbus_message_unref (hasOwnerReply);
    }
    dbus_error_free (&err);

    if (!hasOwner) {
	dbus_connection_unref (this->m_dbusConnection);
	this->m_dbusConnection = nullptr;
	return;
    }

    // Acquire unique DBus service name
    this->m_dbusServiceName = "io.github.linux_wallpaperengine.detector_" + std::to_string (getpid ());
    dbus_error_init (&err);
    const int reqResult = dbus_bus_request_name (
	this->m_dbusConnection, this->m_dbusServiceName.c_str (),
	DBUS_NAME_FLAG_REPLACE_EXISTING | DBUS_NAME_FLAG_DO_NOT_QUEUE, &err
    );
    if (dbus_error_is_set (&err) || reqResult != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
	dbus_error_free (&err);
	dbus_connection_unref (this->m_dbusConnection);
	this->m_dbusConnection = nullptr;
	return;
    }

    // Register DBus handler
    auto dbusFilter = [] (DBusConnection* conn, DBusMessage* msg, void* user_data) -> DBusHandlerResult {
	auto* self = static_cast<WaylandFullScreenDetector*> (user_data);
	if (dbus_message_is_method_call (
		msg, "io.github.linux_wallpaperengine.FullscreenDetector", "SetFullscreenState"
	    )) {
	    dbus_bool_t state = FALSE;
	    DBusError filterErr;
	    dbus_error_init (&filterErr);
	    if (dbus_message_get_args (msg, &filterErr, DBUS_TYPE_BOOLEAN, &state, DBUS_TYPE_INVALID)) {
		self->m_fullscreenCount = (state == TRUE) ? 1 : 0;
	    }
	    dbus_error_free (&filterErr);
	    DBusMessage* reply = dbus_message_new_method_return (msg);
	    if (reply) {
		dbus_connection_send (conn, reply, nullptr);
		dbus_message_unref (reply);
	    }
	    return DBUS_HANDLER_RESULT_HANDLED;
	}
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    };

    dbus_connection_add_filter (this->m_dbusConnection, dbusFilter, this, nullptr);

    // Write KWin script to temp file
    const std::string scriptPath = "/tmp/lwe_kwin_" + std::to_string (getpid ()) + ".js";
    this->m_kwinScriptName = "linux-wallpaperengine-" + std::to_string (getpid ());

    std::string jsCode = R"JS(
(function() {
    var service = ")JS" + this->m_dbusServiceName + R"JS(";
    var path = "/FullscreenDetector";
    var iface = "io.github.linux_wallpaperengine.FullscreenDetector";
    var lastState = null;

    function checkState() {
        var windows = workspace.windowList();
        var hasCovering = false;
        for (var i = 0; i < windows.length; i++) {
            var w = windows[i];
            if (!w) continue;
            if (w.normalWindow === false) continue;
            if (w.minimized || w.hidden) continue;
            if (!w.onAllDesktops && w.desktops && w.desktops.length > 0) {
                var onDesktop = false;
                for (var d = 0; d < w.desktops.length; d++) {
                    if (w.desktops[d] === workspace.currentDesktop) {
                        onDesktop = true;
                        break;
                    }
                }
                if (!onDesktop) continue;
            }
            var isFull = !!w.fullScreen;
            var isMax = (w.maximizeMode === 3);
            if (isFull || isMax) {
                hasCovering = true;
                break;
            }
        }
        if (hasCovering !== lastState) {
            lastState = hasCovering;
            callDBus(service, path, iface, "SetFullscreenState", hasCovering);
        }
    }

    function hookWindow(w) {
        if (!w) return;
        if (w.fullScreenChanged) w.fullScreenChanged.connect(checkState);
        if (w.maximizedChanged) w.maximizedChanged.connect(checkState);
        if (w.minimizedChanged) w.minimizedChanged.connect(checkState);
        if (w.hiddenChanged) w.hiddenChanged.connect(checkState);
    }

    var all = workspace.windowList();
    for (var i = 0; i < all.length; i++) {
        hookWindow(all[i]);
    }

    workspace.windowAdded.connect(function(w) {
        hookWindow(w);
        checkState();
    });

    workspace.windowRemoved.connect(checkState);
    workspace.windowActivated.connect(checkState);
    if (workspace.currentDesktopChanged) {
        workspace.currentDesktopChanged.connect(checkState);
    }

    checkState();
})();
)JS";

    {
	std::ofstream f (scriptPath);
	if (!f.is_open ()) {
	    sLog.out ("KWin detector: failed to write temporary script");
	    return;
	}
	f << jsCode;
    }

    // Load into KWin
    dbus_error_init (&err);
    DBusMessage* loadMsg = dbus_message_new_method_call (
	"org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting", "loadScript"
    );
    const char* cPath = scriptPath.c_str ();
    const char* cName = this->m_kwinScriptName.c_str ();
    dbus_message_append_args (loadMsg, DBUS_TYPE_STRING, &cPath, DBUS_TYPE_STRING, &cName, DBUS_TYPE_INVALID);
    DBusMessage* loadReply
	= dbus_connection_send_with_reply_and_block (this->m_dbusConnection, loadMsg, 2000, &err);
    dbus_message_unref (loadMsg);

    int32_t scriptId = -1;
    if (loadReply && dbus_message_get_args (loadReply, &err, DBUS_TYPE_INT32, &scriptId, DBUS_TYPE_INVALID)) {
	this->m_kwinScriptId = scriptId;
	dbus_message_unref (loadReply);
    } else {
	dbus_error_free (&err);
	unlink (scriptPath.c_str ());
	return;
    }

    // Run script in KWin
    dbus_error_init (&err);
    const std::string scriptObjPath = "/Scripting/Script" + std::to_string (this->m_kwinScriptId);
    DBusMessage* runMsg = dbus_message_new_method_call (
	"org.kde.KWin", scriptObjPath.c_str (), "org.kde.kwin.Script", "run"
    );
    DBusMessage* runReply = dbus_connection_send_with_reply_and_block (this->m_dbusConnection, runMsg, 2000, &err);
    dbus_message_unref (runMsg);
    if (runReply) {
	dbus_message_unref (runReply);
	this->m_kwinActive = true;
	sLog.out ("KWin Wayland fullscreen/maximized detector successfully loaded");
    }
    dbus_error_free (&err);
    unlink (scriptPath.c_str ());

    // Drain initial DBus state
    dbus_connection_read_write (this->m_dbusConnection, 50);
    while (dbus_connection_dispatch (this->m_dbusConnection) == DBUS_DISPATCH_DATA_REMAINS)
	;
}

void WaylandFullScreenDetector::cleanupKWinDetector () {
    if (!this->m_kwinActive || !this->m_dbusConnection) {
	return;
    }

    DBusError err;
    dbus_error_init (&err);

    // Stop script
    if (this->m_kwinScriptId >= 0) {
	const std::string scriptObjPath = "/Scripting/Script" + std::to_string (this->m_kwinScriptId);
	DBusMessage* stopMsg = dbus_message_new_method_call (
	    "org.kde.KWin", scriptObjPath.c_str (), "org.kde.kwin.Script", "stop"
	);
	if (stopMsg) {
	    DBusMessage* stopReply
		= dbus_connection_send_with_reply_and_block (this->m_dbusConnection, stopMsg, 1000, &err);
	    dbus_message_unref (stopMsg);
	    if (stopReply) {
		dbus_message_unref (stopReply);
	    }
	}
	dbus_error_free (&err);
    }

    // Unload script
    if (!this->m_kwinScriptName.empty ()) {
	dbus_error_init (&err);
	DBusMessage* unloadMsg = dbus_message_new_method_call (
	    "org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting", "unloadScript"
	);
	if (unloadMsg) {
	    const char* cName = this->m_kwinScriptName.c_str ();
	    dbus_message_append_args (unloadMsg, DBUS_TYPE_STRING, &cName, DBUS_TYPE_INVALID);
	    DBusMessage* unloadReply
		= dbus_connection_send_with_reply_and_block (this->m_dbusConnection, unloadMsg, 1000, &err);
	    dbus_message_unref (unloadMsg);
	    if (unloadReply) {
		dbus_message_unref (unloadReply);
	    }
	}
	dbus_error_free (&err);
    }

    dbus_connection_unref (this->m_dbusConnection);
    this->m_dbusConnection = nullptr;
    this->m_kwinActive = false;
}

WaylandFullScreenDetector::WaylandFullScreenDetector (Application::ApplicationContext& appContext) :
    FullScreenDetector (appContext), m_callbackData { .detector = this, .fullscreenCount = &m_fullscreenCount } {
    m_display = wl_display_connect (nullptr);
    if (!m_display) {
	sLog.exception ("Failed to query wayland display");
    }

    const auto registry = wl_display_get_registry (m_display);
    wl_registry_add_listener (registry, &registryListener, this);
    wl_display_roundtrip (m_display); // load list of toplevels
    if (!m_toplevelManager) {
	this->initKWinDetector ();
	if (!this->m_kwinActive) {
	    sLog.out ("Fullscreen detection not supported by your Wayland compositor");
	}
    } else {
	wl_display_roundtrip (m_display); // load toplevel details
    }
}

WaylandFullScreenDetector::~WaylandFullScreenDetector () {
    this->cleanupKWinDetector ();
    if (m_display) {
	wl_display_disconnect (m_display);
    }
}

bool WaylandFullScreenDetector::anythingFullscreen () const {
    if (this->m_kwinActive && this->m_dbusConnection) {
	dbus_connection_read_write (this->m_dbusConnection, 0);
	while (dbus_connection_dispatch (this->m_dbusConnection) == DBUS_DISPATCH_DATA_REMAINS)
	    ;
	return this->m_fullscreenCount > 0;
    }

    if (!m_toplevelManager) {
	return false;
    }
    wl_display_roundtrip (m_display);
    return m_fullscreenCount > 0;
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
