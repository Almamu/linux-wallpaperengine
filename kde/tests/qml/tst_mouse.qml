// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../plasma/contents/ui"
Item {
    width: 400; height: 300
    MouseArea {
        id: desktop
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        property int leftClicks: 0
        property int rightClicks: 0
        property int moves: 0
        onClicked: mouse => { if (mouse.button === Qt.LeftButton) leftClicks++; else rightClicks++ }
        onPositionChanged: moves++
    }
    MouseRelay { id: relay; forwarding: true }
    TestCase {
        name: "PassiveDesktopInput"
        when: windowShown
        function test_desktopAndWallpaper() {
            mouseMove(desktop, 100, 90)
            tryCompare(relay, "cursorX", .25)
            mousePress(desktop, 100, 90, Qt.LeftButton)
            compare(desktop.pressed, true)
            compare(relay.leftDown, true)
            mouseMove(desktop, 200, 120)
            compare(desktop.pressed, true)
            compare(relay.leftDown, true)
            verify(desktop.moves > 0)
            mouseRelease(desktop, 200, 120, Qt.LeftButton)
            compare(relay.leftDown, false)
            compare(desktop.leftClicks, 1)
            mousePress(desktop, 100, 90, Qt.RightButton)
            compare(relay.rightDown, true)
            mouseRelease(desktop, 100, 90, Qt.RightButton)
            compare(relay.rightDown, false)
            compare(desktop.rightClicks, 1)
            mousePress(desktop, 50, 50, Qt.LeftButton)
            compare(relay.leftDown, true)
            relay.forwarding = false
            compare(relay.leftDown, false)
            mouseRelease(desktop, 50, 50, Qt.LeftButton)
        }
    }
}
