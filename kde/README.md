# KDE Plasma integration (Plasma / KWin 6.5 or newer, Wayland)

The regular layer-shell `bottom` layer is **above** Plasma's desktop, hiding
icons. Its default pointer input region also consumes desktop clicks. On KDE,
linux-wallpaperengine now uses the background layer and an empty input region.
A small, transparent Plasma wallpaper and a KWin stacking constraint keep the
renderer below the actual desktop. No pixels are copied through the CPU, and
no native code is injected into plasmashell.

## One-time setup

From `build/output/kde`:

```sh
python3 install.py --list
python3 install.py --desktop DESKTOP_ID  # replace with a numeric ID from --list
systemctl --user restart plasma-plasmashell.service
```

The one-time restart is needed because an already-created opaque Qt desktop
cannot acquire a working alpha buffer just by changing its clear color.
Only the selected desktop changes. Repeat installation for additional screens
before restarting Plasma. Do not select an inactive/disconnected containment.
The integration remains selected across login sessions. Start the renderer as
usual, e.g. `linux-wallpaperengine --screen-root DP-1 --bg PATH` or through a GUI.
No extra renderer arguments or recurring Plasma restarts are needed.

Icons, widgets, right-click menus, dragging, selection and panels remain owned
by Plasma. By default, direct mouse interaction/parallax in the external wallpaper is
unavailable in this mode: Wayland does not broadcast the desktop's pointer
input to background clients. Other desktop environments and window previews
keep their existing behavior. `LWE_KDE_DESKTOP=0` opts out of the KDE defaults.

KWin watches renderer windows and shows the saved static fallback on the
matching screen when the renderer stops, including after crashes/SIGKILL.
The previous wallpaper plugin and its original settings are retained. Solid
colors and image paths are used for the fallback; other wallpaper plugins use
a black fallback. A directory-based image wallpaper may need an explicit image
file as `FallbackImage`. Stop the renderer before changing wallpaper type.

To return to the previous wallpaper type:

```sh
python3 install.py --desktop DESKTOP_ID --restore
```

The KWin script can be disabled in System Settings → Window Management → KWin
Scripts. It only manages `linux-wallpaperengine` surfaces and never hides,
minimizes or disables Plasma's desktop. `--layer` is deliberately overridden
in KDE desktop mode to prevent accidental overlays.

## Experimental passive mouse forwarding

Enable separately for each desktop with:

```sh
python3 kde/install.py --desktop ID --mouse-interaction on
```

After upgrading the QML plugin, restart Plasma once to load the new code.
Use `--mouse-interaction off` to disable forwarding. It defaults to off.

The wallpaper surface keeps its empty input region. A visual-free Qt Quick
item observes hover and left/right button states passively inside the Plasma
desktop window and sends copies to a process-specific session D-Bus endpoint.
KWin supplies the renderer PID and screen origin; normalized coordinates are
converted to the renderer's physical pixels and bottom-left origin. Short
press/release pairs are retained across render frames; missing releases expire
after one second without a heartbeat. Each output has independent input state.

This trial observes desktop input only, including icon clicks and selection
drags. Application windows, panels, context-menu contents, wheel scrolling,
keyboard events and touch gestures are outside its scope. The wallpaper itself
must implement mouse interaction for any visual effect to appear.

Tests: `node kde/tests/stacking.test.js`, the standalone D-Bus test in
`kde/tests/mouse-bridge.cpp`, and `QT_QPA_PLATFORM=offscreen qmltestrunner-qt6
-input kde/tests/qml`. The QML tests check that an underlying desktop MouseArea
still receives clicks and dragging while the passive observer sees them too.

Run the native and end-to-end transport tests in an isolated session bus:

```sh
g++ -std=c++20 -Wall -Wextra -Werror -Isrc kde/tests/mouse-bridge.cpp \
  src/WallpaperEngine/Input/Drivers/KdeMouseBridge.cpp \
  $(pkg-config --cflags --libs dbus-1) -o /tmp/lwe-mouse-bridge-test
dbus-run-session -- /tmp/lwe-mouse-bridge-test
dbus-run-session -- python3 kde/tests/mouse-transport.py /tmp/lwe-mouse-bridge-test
```

## Automatic wallpaper restoration

The installer also enables the per-user wallpaper restoration service when a
KDE Wayland/systemd session and `kscreen-doctor` are available. `build.sh` sets up
the same service after a successful build. Both accept `--no-autostart` to skip
setup; this does not disable an existing service.

The tracker watches frontend and Plasma configuration changes using inotify.
It performs no periodic process scans, writes state only when commands change,
and takes one final snapshot when the user session stops. The initial login
restore waits up to 90 seconds for screens and skips already running wallpapers.
Saved selections survive renderer exits, so a manually stopped wallpaper is
still remembered for the next login.

For an existing build, run `python3 kde/autostart/install.py --binary
"$PWD/build/output/linux-wallpaperengine"` from the source directory. To disable
restoration and stop the tracker, run
`systemctl --user disable --now linux-wallpaperengine-restore.service`.
This also stops wallpaper processes launched by the service. Reinstalling does
not restart an active tracker; updated code is used on the next login.
