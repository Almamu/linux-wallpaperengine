#include "KDEWaylandFullScreenDetector.h"

#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Render/Drivers/VideoFactories.h"

#include <cstdlib>
#include <cstring>
#include <sstream>

#include <algorithm>
#include <cctype>
#include <string>

namespace WallpaperEngine::Render::Drivers::Detectors {

namespace {
    constexpr const char* defaultService = "org.linuxwallpaperengine.WaylandDetector";
    constexpr const char* defaultObject = "/org/linuxwallpaperengine/WaylandDetector";
    constexpr const char* method = "OnWindowChanged";

    std::string toStringSafe (const char* value) {
	if (value == nullptr) {
	    return {};
	}

	return value;
    }

} // namespace

const char* KDEWaylandFullScreenDetector::defaultServiceName () { return defaultService; }

const char* KDEWaylandFullScreenDetector::objectPath () { return defaultObject; }

const char* KDEWaylandFullScreenDetector::methodName () { return method; }

KDEWaylandFullScreenDetector::KDEWaylandFullScreenDetector (Application::ApplicationContext& appContext) :
    FullScreenDetector (appContext), m_serviceName (toStringSafe (std::getenv ("KWIN_MAXIMIZE_DETECTOR_DBUS_SERVICE"))) {
    if (m_serviceName.empty ()) {
	m_serviceName = defaultServiceName ();
    }

    if (!initializeDBus ()) {
	sLog.out (
	    "KDE Wayland maximize detector could not initialize DBus. Falling back to the Wayland fullscreen detector"
	);
    }
}

KDEWaylandFullScreenDetector::~KDEWaylandFullScreenDetector () { stopDBus (); }

bool KDEWaylandFullScreenDetector::initializeDBus () {
    dbus_threads_init_default ();

    DBusError error;
    dbus_error_init (&error);

    m_connection = dbus_bus_get (DBUS_BUS_SESSION, &error);
    if (dbus_error_is_set (&error)) {
	sLog.error ("Failed to connect to the session DBus: ", error.message);
	dbus_error_free (&error);
	return false;
    }

    if (m_connection == nullptr) {
	sLog.error ("Failed to connect to the session DBus: unknown error");
	return false;
    }

    dbus_connection_set_exit_on_disconnect (m_connection, false);

    dbus_error_init (&error);
    const auto requestResult
	= dbus_bus_request_name (m_connection, m_serviceName.c_str (), DBUS_NAME_FLAG_DO_NOT_QUEUE, &error);

    if (dbus_error_is_set (&error)) {
	sLog.error ("Failed to request DBus service ", m_serviceName, ": ", error.message);
	dbus_error_free (&error);
	dbus_connection_unref (m_connection);
	m_connection = nullptr;
	return false;
    }

    if (requestResult != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER
	&& requestResult != DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER) {
	sLog.error ("DBus service ", m_serviceName, " is already owned by another process");
	dbus_connection_unref (m_connection);
	m_connection = nullptr;
	return false;
    }

    static constexpr DBusObjectPathVTable objectVTable = {
	.unregister_function = nullptr,
	.message_function = &KDEWaylandFullScreenDetector::handleMessage,
    };

    if (!dbus_connection_register_object_path (m_connection, objectPath (), &objectVTable, this)) {
	sLog.error ("Failed to register DBus object path for KDE Wayland maximize detector");
	dbus_connection_unref (m_connection);
	m_connection = nullptr;
	return false;
    }

    m_dispatchThread = std::thread (&KDEWaylandFullScreenDetector::dispatchLoop, this);
    return true;
}

void KDEWaylandFullScreenDetector::stopDBus () {
    m_stop.store (true, std::memory_order_release);

    if (m_dispatchThread.joinable ()) {
	m_dispatchThread.join ();
    }

    if (m_connection != nullptr) {
	dbus_connection_unregister_object_path (m_connection, objectPath ());
	dbus_connection_unref (m_connection);
	m_connection = nullptr;
    }
}

void KDEWaylandFullScreenDetector::dispatchLoop () {
    while (!m_stop.load (std::memory_order_acquire)) {
	if (m_connection == nullptr) {
	    break;
	}

	dbus_connection_read_write_dispatch (m_connection, 250);
    }
}

DBusHandlerResult
KDEWaylandFullScreenDetector::handleMessage (DBusConnection* connection, DBusMessage* message, void* userData) {
    const auto detector = static_cast<KDEWaylandFullScreenDetector*> (userData);
    return detector->handleMessage (message);
}

DBusHandlerResult KDEWaylandFullScreenDetector::handleMessage (DBusMessage* message) {
    if (message == nullptr) {
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (dbus_message_get_type (message) != DBUS_MESSAGE_TYPE_METHOD_CALL) {
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (const auto* currentInterface = dbus_message_get_interface (message);
	currentInterface == nullptr || std::strcmp (currentInterface, m_serviceName.c_str ()) != 0) {
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    return handleMethodCall (message) ? DBUS_HANDLER_RESULT_HANDLED : DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

bool KDEWaylandFullScreenDetector::handleMethodCall (DBusMessage* message) {
    const auto* member = dbus_message_get_member (message);
    if (member == nullptr) {
	return false;
    }

    if (std::strcmp (member, methodName ()) != 0) {
	return false;
    }

    DBusError error;
    dbus_error_init (&error);

    const char* windowKey = nullptr;
    const char* windowName = nullptr;
    const char* appId = nullptr;
    dbus_int32_t pid = 0;
    dbus_bool_t horizontal = false;
    dbus_bool_t vertical = false;
    dbus_bool_t fully = false;

    if (!dbus_message_get_args (
	    message, &error, DBUS_TYPE_STRING, &windowKey, DBUS_TYPE_STRING, &windowName, DBUS_TYPE_INT32, &pid,
	    DBUS_TYPE_STRING, &appId, DBUS_TYPE_BOOLEAN, &horizontal, DBUS_TYPE_BOOLEAN, &vertical, DBUS_TYPE_BOOLEAN,
	    &fully, DBUS_TYPE_INVALID
	)) {
	sLog.error ("Invalid KDE Wayland maximize DBus call: ", error.message);
	dbus_error_free (&error);
	return false;
    }

    updateWindowState (toStringSafe (windowKey), toStringSafe (appId), horizontal != false, vertical != false, fully != false);

    if (m_connection != nullptr) {
	DBusMessage* reply = dbus_message_new_method_return (message);
	if (reply != nullptr) {
	    dbus_connection_send (m_connection, reply, nullptr);
	    dbus_message_unref (reply);
	}
    }

    return true;
}

bool KDEWaylandFullScreenDetector::updateWindowState (
    const std::string& windowKey, const std::string& appId, bool horizontal, bool vertical, bool fully
) {
    std::lock_guard lock (m_stateMutex);
    m_windowStates.insert_or_assign (
	windowKey,
	WindowState {
	    .horizontal = horizontal,
	    .vertical = vertical,
	    .fully = fully,
	    .appId = appId,
	}
    );

    if (!windowKey.empty ()) {
	m_activeWindowKey = windowKey;
    }
    return true;
}

bool KDEWaylandFullScreenDetector::anythingFullscreen () const {
    const auto& ctx = getApplicationContext ();

    auto isRelevant = [&ctx] (const WindowState& state) {
	if (!state.fully) {
	    return false;
	}

	if (!state.appId.empty()) {
    std::string appIdLower = state.appId;
    std::transform(appIdLower.begin(), appIdLower.end(), appIdLower.begin(),
        [](unsigned char c) { return std::tolower(c); });

    for (const auto& ignore : ctx.settings.render.fullscreenPauseIgnoreAppIds) {
        if (ignore.empty()) {
            continue;
        }

        std::string ignoreLower = ignore;
        std::transform(ignoreLower.begin(), ignoreLower.end(), ignoreLower.begin(),
            [](unsigned char c) { return std::tolower(c); });

        if (appIdLower.find(ignoreLower) != std::string::npos) {
            return false;
        }
    }
}

	return true;
    };

    std::lock_guard lock (m_stateMutex);

    if (ctx.settings.render.pauseOnFullscreenOnlyWhenActive) {
	const auto activeIt = m_windowStates.find (m_activeWindowKey);
	return activeIt != m_windowStates.end () && isRelevant (activeIt->second);
    }

    for (const auto& entry : m_windowStates) {
	if (isRelevant (entry.second)) {
	    return true;
	}
    }

    return false;
}

bool KDEWaylandFullScreenDetector::isInitialized () const { return m_connection != nullptr; }

void KDEWaylandFullScreenDetector::reset () {
    {
	std::lock_guard lock (m_stateMutex);
	m_windowStates.clear ();
	m_activeWindowKey.clear ();
    }
}

} // namespace WallpaperEngine::Render::Drivers::Detectors