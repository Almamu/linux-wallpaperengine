#pragma once

#ifdef ENABLE_WAYLAND

#include <atomic>
#include <dbus/dbus.h>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "FullScreenDetector.h"
#include "WaylandFullScreenDetector.h"

namespace WallpaperEngine::Render::Drivers::Detectors {

class KDEWaylandFullScreenDetector final : public FullScreenDetector {
public:
    explicit KDEWaylandFullScreenDetector (Application::ApplicationContext& appContext);
    ~KDEWaylandFullScreenDetector () override;

    [[nodiscard]] bool anythingFullscreen () const override;
    void reset () override;

private:
    struct WindowState {
        bool horizontal = false;
        bool vertical = false;
        bool fully = false;
    std::string appId {};
    };

    static const char* defaultServiceName ();
    static const char* objectPath ();
    static const char* methodName ();
    
    static DBusHandlerResult handleMessage (DBusConnection* connection, DBusMessage* message, void* userData);
    DBusHandlerResult handleMessage (DBusMessage* message);
    bool initializeDBus ();
    void stopDBus ();
    void dispatchLoop ();
    bool handleMethodCall (DBusMessage* message);
    bool updateWindowState (
	const std::string& windowKey, const std::string& appId, bool horizontal, bool vertical, bool fully
    );
    [[nodiscard]] bool anythingFullscreenFromDbus () const;

    std::unique_ptr<WaylandFullScreenDetector> m_fallbackDetector;
    DBusConnection* m_connection = nullptr;
    std::thread m_dispatchThread;
    std::atomic_bool m_stop {false};
    std::atomic_bool m_hasReceivedUpdate {false};
    mutable std::mutex m_stateMutex;
    std::unordered_map<std::string, WindowState> m_windowStates;
    std::string m_activeWindowKey;
    std::string m_serviceName;
};

} // namespace WallpaperEngine::Render::Drivers::Detectors

#endif /* ENABLE_WAYLAND */