// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Window
import org.kde.plasma.plasmoid

WallpaperItem {
    id: root

    MouseRelay {
        parent: root.Window.window ? root.Window.window.contentItem : null
        visible: root.visible
        forwarding: root.configuration.MouseRelayEnabled && root.configuration.Active
        targetsJson: root.configuration.InputTargets || "[]"
    }

    // Plasma's DesktopView clears to black by default. Only the wallpaper is
    // transparent: icons, widgets, context menus and pointer input stay intact.
    Binding {
        target: root.Window.window
        property: "color"
        value: "transparent"
        when: root.Window.window !== null
        restoreMode: Binding.RestoreBindingOrValue
    }

    // KWin updates Active when renderer windows appear/disappear. Keeping this
    // plugin selected also creates an alpha-capable desktop at the next login.
    Rectangle {
        anchors.fill: parent
        visible: !root.configuration.Active
        color: root.configuration.FallbackColor
        Image {
            anchors.fill: parent
            source: root.configuration.FallbackImage
            fillMode: Image.PreserveAspectCrop
        }
    }
}
