// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <dbus/dbus.h>
#include <chrono>
#include <deque>
#include <map>
#include <string>

namespace WallpaperEngine::Input::Drivers {
// Receives copies of desktop input; never grabs a device or a Wayland surface.
class KdeMouseBridge {
public:
    struct State { double x = 0.5, y = 0.5; bool left = false, right = false; };
    KdeMouseBridge ();
    ~KdeMouseBridge ();
    KdeMouseBridge (const KdeMouseBridge&) = delete;
    KdeMouseBridge& operator= (const KdeMouseBridge&) = delete;
    void poll ();
    State frame (int screenX, int screenY);
    const std::string& service () const { return m_service; }
private:
    struct Screen {
        State current, latest;
        std::deque<State> transitions;
        std::chrono::steady_clock::time_point received;
    };
    DBusConnection* m_connection = nullptr;
    std::string m_service;
    std::map<std::pair<int, int>, Screen> m_screens;
    dbus_uint32_t m_events = 0;
    void handle (DBusMessage* message);
};
}
