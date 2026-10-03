// SPDX-License-Identifier: GPL-3.0-or-later
// Compile with KdeMouseBridge.cpp and pkg-config --cflags --libs dbus-1.
// Run under dbus-run-session, so no real desktop is involved.
#include "WallpaperEngine/Input/Drivers/KdeMouseBridge.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <thread>
using WallpaperEngine::Input::Drivers::KdeMouseBridge;
int main (int argc, char**) {
    KdeMouseBridge bridge;
    DBusConnection* client = dbus_bus_get_private (DBUS_BUS_SESSION, nullptr);
    assert (client);
    if (argc > 1) {
        std::cout << bridge.service () << std::endl;
        for (int i = 0; i < 15000; ++i) {
            bridge.poll ();
            std::this_thread::sleep_for (std::chrono::milliseconds (2));
        }
        return 0;
    }
    auto send = [&] (int sx, int sy, double x, double y, bool l, bool r, bool valid = true) {
        auto* msg = dbus_message_new_method_call (bridge.service ().c_str (), "/org/linuxwallpaperengine/Mouse", "org.linuxwallpaperengine.Mouse", "Update");
        dbus_int32_t ix = sx, iy = sy;
        dbus_bool_t left = l, right = r;
        dbus_message_append_args (msg, DBUS_TYPE_INT32, &ix, DBUS_TYPE_INT32, &iy,
            DBUS_TYPE_DOUBLE, &x, DBUS_TYPE_DOUBLE, &y, DBUS_TYPE_BOOLEAN, &left,
            DBUS_TYPE_BOOLEAN, &right, DBUS_TYPE_INVALID);
        DBusPendingCall* pending = nullptr;
        assert (dbus_connection_send_with_reply (client, msg, &pending, 1000));
        dbus_message_unref (msg);
        dbus_connection_flush (client);
        for (int i = 0; i < 1000 && !dbus_pending_call_get_completed (pending); ++i) {
            bridge.poll ();
            dbus_connection_read_write_dispatch (client, 1);
        }
        assert (dbus_pending_call_get_completed (pending));
        auto* reply = dbus_pending_call_steal_reply (pending);
        assert (reply);
        assert ((dbus_message_get_type (reply) == DBUS_MESSAGE_TYPE_METHOD_RETURN) == valid);
        dbus_message_unref (reply);
        dbus_pending_call_unref (pending);
    };
    assert (bridge.frame (0, 0).x == .5);
    send (-1920, 294, .25, .75, false, false);
    auto a = bridge.frame (-1920, 294);
    assert (a.x == .25 && a.y == .75 && !a.left);
    send (-1920, 294, .1, .2, true, false);
    send (-1920, 294, .2, .3, false, false);
    assert (bridge.frame (-1920, 294).left); // short click survives coalescing
    assert (!bridge.frame (-1920, 294).left);
    send (2560, 0, 0, 1, true, true);
    a = bridge.frame (2560, 0);
    assert (a.x == 0 && a.y == 1 && a.left && a.right);
    assert (!bridge.frame (-1920, 294).right); // output isolation
    send (2560, 0, std::numeric_limits<double>::quiet_NaN (), .5, false, false, false);
    send (2560, 0, 1.01, .5, false, false, false);
    assert (bridge.frame (2560, 0).left);
    std::this_thread::sleep_for (std::chrono::milliseconds (1100));
    a = bridge.frame (2560, 0);
    assert (!a.left && !a.right); // crash or missing release
    dbus_connection_close (client);
    dbus_connection_unref (client);
    std::cout << "PASS: D-Bus transport, validation, short clicks, output isolation, timeout release\n";
}
