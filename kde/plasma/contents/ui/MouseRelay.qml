// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import org.kde.plasma.workspace.dbus as DBus

// Visual-free, passive observers in the desktop window's content item. No
// MouseArea, TapHandler, focus request or exclusive pointer grab is used.
Item {
    id: relay
    property bool forwarding: false
    property string targetsJson: "[]"
    readonly property var targets: {
        try { const result = JSON.parse(targetsJson); return Array.isArray(result) ? result : [] } catch (e) { return [] }
    }
    property real cursorX: 0.5
    property real cursorY: 0.5
    property bool leftDown: false
    property bool rightDown: false
    property bool dirty: false
    property bool failed: false
    property int inFlight: 0
    readonly property bool observing: forwarding && visible && width > 0 && height > 0
    anchors.fill: parent
    z: 1000000

    function position(point) {
        cursorX = Math.max(0, Math.min(1, point.x / width))
        cursorY = Math.max(0, Math.min(1, point.y / height))
        dirty = true
    }
    function send(release) {
        if ((!forwarding && !release) || inFlight > 32) return
        dirty = false
        for (const target of targets) {
            if (!/^org\.linuxwallpaperengine\.Mouse\.p[0-9]+$/.test(target.service)) continue
            ++inFlight
            DBus.SessionBus.asyncCall({
                service: target.service,
                path: "/org/linuxwallpaperengine/Mouse",
                iface: "org.linuxwallpaperengine.Mouse",
                member: "Update",
                arguments: [new DBus.int32(target.x), new DBus.int32(target.y), new DBus.double(cursorX), new DBus.double(cursorY),
                            new DBus.bool(release ? false : leftDown), new DBus.bool(release ? false : rightDown)]
            }, function() { --relay.inFlight; relay.failed = false },
               function(error) {
                   --relay.inFlight
                   if (!relay.failed) console.warn("Wallpaper mouse relay:", error.error.name, error.error.message)
                   relay.failed = true
               })
        }
    }
    function release() {
        leftDown = false
        rightDown = false
        send(true)
    }
    onObservingChanged: { if (!observing) release() }
    Component.onDestruction: release()

    HoverHandler {
        id: hover
        enabled: relay.observing
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        blocking: false
        onPointChanged: { if (hover.hovered) relay.position(hover.point.position) }
        onHoveredChanged: { if (!hovered && !left.active && !right.active) relay.release() }
    }
    // Separate parents let both observers follow the same mouse point, including
    // chords. PointHandler observes passively even if Plasma takes an exclusive grab.
    Item {
        anchors.fill: parent
        PointHandler {
            id: left
            enabled: relay.observing
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            acceptedButtons: Qt.LeftButton
            onPointChanged: { if (active) relay.position(point.position) }
            onActiveChanged: {
                if (active) relay.position(point.position)
                relay.leftDown = active
                relay.send(false)
            }
        }
    }
    Item {
        anchors.fill: parent
        PointHandler {
            id: right
            enabled: relay.observing
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            acceptedButtons: Qt.RightButton
            onPointChanged: { if (active) relay.position(point.position) }
            onActiveChanged: {
                if (active) relay.position(point.position)
                relay.rightDown = active
                relay.send(false)
            }
        }
    }
    Timer {
        interval: 16
        repeat: true
        running: relay.observing
        onTriggered: { if (relay.dirty) relay.send(false) }
    }
    Timer {
        interval: 250
        repeat: true
        running: relay.observing && (relay.leftDown || relay.rightDown)
        onTriggered: relay.send(false)
    }
}
