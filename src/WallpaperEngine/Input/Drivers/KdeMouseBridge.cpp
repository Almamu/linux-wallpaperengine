// SPDX-License-Identifier: GPL-3.0-or-later
#include "KdeMouseBridge.h"
#include <cmath>
#include <unistd.h>

using namespace WallpaperEngine::Input::Drivers;
namespace {
constexpr const char* path = "/org/linuxwallpaperengine/Mouse";
constexpr const char* iface = "org.linuxwallpaperengine.Mouse";
constexpr const char* introspection = R"(<node>
<interface name="org.linuxwallpaperengine.Mouse">
<method name="Update"><arg type="i" direction="in"/><arg type="i" direction="in"/>
<arg type="d" direction="in"/><arg type="d" direction="in"/>
<arg type="b" direction="in"/><arg type="b" direction="in"/></method>
<method name="GetEventCount"><arg type="u" direction="out"/></method></interface>
<interface name="org.freedesktop.DBus.Introspectable"><method name="Introspect">
<arg type="s" direction="out"/></method></interface></node>)";
}
KdeMouseBridge::KdeMouseBridge () : m_service ("org.linuxwallpaperengine.Mouse.p" + std::to_string (getpid ())) {
    // A private connection prevents interference with the media player's bus dispatcher.
    DBusError error;
    dbus_error_init (&error);
    m_connection = dbus_bus_get_private (DBUS_BUS_SESSION, &error);
    dbus_error_free (&error);
    if (!m_connection) return;
    dbus_connection_set_exit_on_disconnect (m_connection, false);
    const int result = dbus_bus_request_name (m_connection, m_service.c_str (), DBUS_NAME_FLAG_DO_NOT_QUEUE, &error);
    dbus_error_free (&error);
    if (result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        dbus_connection_close (m_connection);
        dbus_connection_unref (m_connection);
        m_connection = nullptr;
    }
}
KdeMouseBridge::~KdeMouseBridge () {
    if (!m_connection) return;
    dbus_connection_close (m_connection);
    dbus_connection_unref (m_connection);
}
void KdeMouseBridge::poll () {
    if (!m_connection) return;
    dbus_connection_read_write (m_connection, 0);
    // Bound work per render cycle, even if a client sends excessive input.
    for (int i = 0; i < 256; ++i) {
        DBusMessage* message = dbus_connection_pop_message (m_connection);
        if (!message) break;
        handle (message);
        dbus_message_unref (message);
    }
}
void KdeMouseBridge::handle (DBusMessage* message) {
    if (dbus_message_get_type (message) != DBUS_MESSAGE_TYPE_METHOD_CALL) return;
    DBusMessage* reply = nullptr;
    if (!dbus_message_has_path (message, path)) {
        reply = dbus_message_new_error (message, DBUS_ERROR_UNKNOWN_OBJECT, "Unknown input object");
    } else if (dbus_message_is_method_call (message, DBUS_INTERFACE_INTROSPECTABLE, "Introspect")) {
        reply = dbus_message_new_method_return (message);
        dbus_message_append_args (reply, DBUS_TYPE_STRING, &introspection, DBUS_TYPE_INVALID);
    } else if (dbus_message_is_method_call (message, iface, "GetEventCount")) {
        reply = dbus_message_new_method_return (message);
        dbus_message_append_args (reply, DBUS_TYPE_UINT32, &m_events, DBUS_TYPE_INVALID);
    } else if (dbus_message_is_method_call (message, iface, "Update")) {
        dbus_int32_t sx, sy;
        double x, y;
        dbus_bool_t left, right;
        if (!dbus_message_has_signature (message, "iiddbb")
            || !dbus_message_get_args (message, nullptr, DBUS_TYPE_INT32, &sx, DBUS_TYPE_INT32, &sy,
                DBUS_TYPE_DOUBLE, &x, DBUS_TYPE_DOUBLE, &y, DBUS_TYPE_BOOLEAN, &left,
                DBUS_TYPE_BOOLEAN, &right, DBUS_TYPE_INVALID)
            || !std::isfinite (x) || !std::isfinite (y) || x < 0 || x > 1 || y < 0 || y > 1) {
            reply = dbus_message_new_error (message, DBUS_ERROR_INVALID_ARGS, "Expected screen origin, normalized x/y and two buttons");
        } else if (m_screens.size () >= 32 && !m_screens.contains ({sx, sy})) {
            reply = dbus_message_new_error (message, DBUS_ERROR_LIMITS_EXCEEDED, "Too many outputs");
        } else {
            auto& screen = m_screens[{sx, sy}];
            const State next {x, y, bool (left), bool (right)};
            if (next.left != screen.latest.left || next.right != screen.latest.right) {
                // Preserve a press+release arriving in the same frame. Each button
                // transition is visible for one rendered frame, independently per output.
                if (screen.transitions.size () >= 64) screen.transitions.clear ();
                screen.transitions.push_back (next);
            }
            screen.latest = next;
            screen.received = std::chrono::steady_clock::now ();
            ++m_events;
            reply = dbus_message_new_method_return (message);
        }
    } else {
        reply = dbus_message_new_error (message, DBUS_ERROR_UNKNOWN_METHOD, "Unknown input method");
    }
    if (reply) {
        if (!dbus_message_get_no_reply (message)) dbus_connection_send (m_connection, reply, nullptr);
        dbus_message_unref (reply);
    }
}
KdeMouseBridge::State KdeMouseBridge::frame (int screenX, int screenY) {
    const auto it = m_screens.find ({screenX, screenY});
    if (it == m_screens.end ()) return {};
    auto& screen = it->second;
    if (std::chrono::steady_clock::now () - screen.received > std::chrono::milliseconds (1000)) {
        // Release on shell crashes, lost focus, disconnection or paused rendering.
        screen.transitions.clear ();
        screen.latest.left = screen.latest.right = false;
    }
    if (!screen.transitions.empty ()) {
        screen.current = screen.transitions.front ();
        screen.transitions.pop_front ();
    } else {
        screen.current = screen.latest;
    }
    return screen.current;
}
