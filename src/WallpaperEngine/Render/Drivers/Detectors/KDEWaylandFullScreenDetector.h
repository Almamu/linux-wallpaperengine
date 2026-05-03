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

/**
 * @brief KDE Plasma/KWin fullscreen and maximize detector using D-Bus.
 *
 * Registers itself as a D-Bus service on the session bus so that a companion
 * KWin script can call @c OnWindowChanged whenever a window's maximize or
 * fullscreen state changes. Incoming notifications are stored per-window and
 * queried by @c anythingFullscreen() to decide whether the wallpaper engine
 * should pause rendering.
 *
 * If D-Bus initialization fails, @c m_connection is left as @c nullptr and
 * the caller is expected to fall back to @c WaylandFullScreenDetector instead.
 *
 * @note Only compiled when @c ENABLE_WAYLAND and @c ENABLE_KDE_EXPERIMENTAL_FEATURES are defined.
 */
class KDEWaylandFullScreenDetector final : public FullScreenDetector {
public:
    explicit KDEWaylandFullScreenDetector (Application::ApplicationContext& appContext);

    /**
     * @brief Destructor. Stops the D-Bus dispatch thread and releases the
     *        session-bus connection.
     */
    ~KDEWaylandFullScreenDetector () override;

    /**
     * @brief Returns @c true if any relevant window is currently fullscreen or
     *        fully maximized.
     *
     * Queries the window-state map populated by @c OnWindowChanged calls from
     * the KWin script. If no notifications have arrived (e.g. no window is
     * fullscreen, or no state has changed since startup), returns @c false.
     *
     * When @c pauseOnFullscreenOnlyWhenActive is set in the render settings,
     * only the most-recently-activated window is examined; otherwise all
     * tracked windows are checked.
     *
     * App IDs listed in @c fullscreenPauseIgnoreAppIds are excluded from
     * consideration (substring match).
     *
     * @return @c true if at least one non-ignored fully-maximized or
     *         fullscreen window exists under the current pause policy.
     */
    [[nodiscard]] bool anythingFullscreen () const override;

    bool isInitialized() const;

    /**
     * @brief Clears all cached window states.
     *
     * After this call @c anythingFullscreen() will return @c false until the
     * next @c OnWindowChanged notification arrives.
     */
    void reset () override;

private:
    /**
     * @brief Snapshot of the maximize/fullscreen state for a single window.
     */
    struct WindowState {
	bool horizontal = false; ///< Window spans the full horizontal extent of its output.
	bool vertical = false; ///< Window spans the full vertical extent of its output.
	bool fully = false; ///< Window is fully maximized or in true fullscreen mode.
	std::string appId {}; ///< Wayland app-id / desktop file name, used for ignore-list matching.
    };

    /** @return @c "org.linuxwallpaperengine.WaylandDetector" */
    static const char* defaultServiceName ();

    /** @return @c "/org/linuxwallpaperengine/WaylandDetector" */
    static const char* objectPath ();

    /** @return @c "OnWindowChanged" */
    static const char* methodName ();

    /**
     * @brief Static C-linkage trampoline required by the libdbus object-path vtable.
     *
     * Casts @p userData back to a @c KDEWaylandFullScreenDetector pointer and
     * delegates to the non-static overload.
     *
     * @param connection  The D-Bus connection that received the message.
     * @param message     The incoming D-Bus message.
     * @param userData    Pointer to the owning @c KDEWaylandFullScreenDetector instance.
     * @return @c DBUS_HANDLER_RESULT_HANDLED if the message was consumed,
     *         @c DBUS_HANDLER_RESULT_NOT_YET_HANDLED otherwise.
     */
    static DBusHandlerResult handleMessage (DBusConnection* connection, DBusMessage* message, void* userData);

    /**
     * @brief Instance-level message handler. Filters by message type and
     *        interface name before forwarding to @c handleMethodCall().
     *
     * @param message  The incoming D-Bus message.
     * @return @c DBUS_HANDLER_RESULT_HANDLED if the message was consumed,
     *         @c DBUS_HANDLER_RESULT_NOT_YET_HANDLED otherwise.
     */
    DBusHandlerResult handleMessage (DBusMessage* message);

    /**
     * @brief Connects to the session D-Bus, requests the service name, registers
     *        the object path, and starts the dispatch thread.
     *
     * @return @c true on success; @c false if any D-Bus operation fails, in
     *         which case the connection is released and @c m_connection is left
     *         as @c nullptr.
     */
    bool initializeDBus ();

    /**
     * @brief Signals the dispatch thread to stop and joins it, then releases
     *        the D-Bus connection.
     */
    void stopDBus ();

    /**
     * @brief Runs the D-Bus dispatch loop on a dedicated thread.
     *
     * Calls @c dbus_connection_read_write_dispatch() in a 250 ms polling loop
     * until @c m_stop is set.
     */
    void dispatchLoop ();

    /**
     * @brief Parses and handles an @c OnWindowChanged method-call message.
     *
     * Expected arguments:
     *   - @c STRING  windowKey
     *   - @c STRING  windowName
     *   - @c INT32   pid
     *   - @c STRING  appId
     *   - @c BOOLEAN horizontal
     *   - @c BOOLEAN vertical
     *   - @c BOOLEAN fully
     *
     * @param message  A D-Bus method-call message.
     * @return @c true if the message was successfully parsed and handled.
     */
    bool handleMethodCall (DBusMessage* message);

    /**
     * @brief Inserts or replaces the @c WindowState entry for @p windowKey
     *        and updates the active-window key.
     *
     * Thread-safe: acquires @c m_stateMutex for the duration of the update.
     *
     * @param windowKey   Opaque string identifier for the window.
     * @param appId       Wayland app-id used for ignore-list matching.
     * @param horizontal  Whether the window occupies the full screen width.
     * @param vertical    Whether the window occupies the full screen height.
     * @param fully       Whether the window is fully maximized or fullscreen.
     */
    bool updateWindowState (
	const std::string& windowKey, const std::string& appId, bool horizontal, bool vertical, bool fully
    );

    /** @brief Session D-Bus connection handle; @c nullptr if initialization failed. */
    DBusConnection* m_connection = nullptr;

    /** @brief Background thread running the D-Bus dispatch loop. */
    std::thread m_dispatchThread;

    /** @brief Set to @c true to signal @c dispatchLoop() to exit cleanly. */
    std::atomic_bool m_stop { false };

    /** @brief Guards @c m_windowStates and @c m_activeWindowKey. */
    mutable std::mutex m_stateMutex;

    /** @brief Per-window maximize/fullscreen state, keyed by the window's opaque identifier. */
    std::unordered_map<std::string, WindowState> m_windowStates;

    /** @brief Key of the most recently activated (focused) window. */
    std::string m_activeWindowKey;

    /**
     * @brief D-Bus well-known service name to register under.
     *
     * Defaults to @c defaultServiceName() unless overridden by the
     * @c KWIN_MAXIMIZE_DETECTOR_DBUS_SERVICE environment variable.
     */
    std::string m_serviceName;
};

} // namespace WallpaperEngine::Render::Drivers::Detectors

#endif /* ENABLE_WAYLAND */