# GNOME Wayland extension

GNOME does not provide the layer-shell protocol used by the engine's Wayland
background mode. This companion extension starts the engine in a native Wayland
window and places its surface in GNOME's background group.

Currently supported: **GNOME Shell 42 on Wayland**. A single renderer shows the same wallpaper on
all monitors. Playback is muted and mouse interaction is disabled. A fullscreen
window hides the background on its monitor; playback pauses when every monitor
is covered. Lock screen backgrounds can use different projects on each monitor,
while keeping GNOME's clock and password prompt. Dynamic backgrounds are shown
without the blur that GNOME applies to still wallpapers. Disabling the extension stops its renderers and reveals
the original background without changing GNOME's wallpaper settings.

## Install

Build or install a Wayland-capable `linux-wallpaperengine` first. GLFW 3.4 was used
for testing; the Ubuntu 22.04 GLFW 3.3.6 package encountered an xdg-shell surface
initialization error. This extension does not install the engine or its libraries.

From the repository root:

```sh
mkdir -p build/gnome
gnome-extensions pack packaging/gnome --force --out-dir=build/gnome
gnome-extensions install --force build/gnome/linux-wallpaperengine@almamu.github.io.shell-extension.zip
```

Log out and log in once so GNOME discovers the new extension. Save your work first.

## Configure and run

Use absolute paths below; GSettings does not expand `~` inside a stored string.
`engine-path` can point to a wrapper that sets up library paths and then `exec`s
the engine. Arguments are passed directly to the executable, not through a shell.

```sh
schema_dir="${XDG_DATA_HOME:-$HOME/.local/share}/gnome-shell/extensions/linux-wallpaperengine@almamu.github.io/schemas"
schema=org.gnome.shell.extensions.linux-wallpaperengine

gsettings --schemadir "$schema_dir" set "$schema" engine-path '/absolute/path/to/linux-wallpaperengine'
gsettings --schemadir "$schema_dir" set "$schema" wallpaper '/absolute/path/to/wallpaper-project'
# Optional if the engine cannot locate the Wallpaper Engine assets automatically:
gsettings --schemadir "$schema_dir" set "$schema" assets-dir '/absolute/path/to/assets'
gnome-extensions enable linux-wallpaperengine@almamu.github.io
```

The wallpaper value can also be a Steam workshop ID. Leave `assets-dir` empty to
use the engine's normal asset discovery. No wallpaper files are bundled here.

```sh
gsettings --schemadir "$schema_dir" set "$schema" fps 30
gsettings --schemadir "$schema_dir" set "$schema" paused true
gsettings --schemadir "$schema_dir" set "$schema" paused false
gnome-extensions disable linux-wallpaperengine@almamu.github.io
```

`paused false` releases manual pause; fullscreen windows covering every monitor
can still keep desktop playback paused. Lock screen playback ignores desktop
fullscreen windows. The extension remains enabled in GNOME's `unlock-dialog` mode.

Configure lock backgrounds in GNOME monitor index order (0, 1, ...):

```sh
gsettings --schemadir "$schema_dir" set "$schema" lock-wallpapers "['/absolute/path/to/first-project', '/absolute/path/to/second-project']"
# Optional horizontal mirroring per monitor:
gsettings --schemadir "$schema_dir" set "$schema" lock-mirrors '[false, true]'
# Keep the locked display on instead of GNOME's immediate fade/blank:
gsettings --schemadir "$schema_dir" set "$schema" keep-lock-screen-on true
```

`keep-lock-screen-on` defaults to false. When enabled, the extension suppresses
the lock fade and sends GNOME's display-wake signal every ten seconds while
locked. Unlocking or disabling the extension stops those requests. Authentication
and automatic locking remain managed by GNOME. Avoid enabling another extension
that also replaces `UnlockDialog` backgrounds.

An empty list or empty entry uses the desktop wallpaper. Identical project values
share one renderer, including the desktop project; different values start separate
renderers while the lock screen is present. Unused desktop playback pauses during
lock, and lock-only renderers stop after unlocking. Lock settings update only lock
backgrounds. Other settings and monitor changes restart playback. Save your work
and log out/in after updating the extension so GNOME loads its new code and session modes.

If playback does not start, check `gnome-extensions info
linux-wallpaperengine@almamu.github.io` and `journalctl --user -b` for engine or
extension errors. An empty wallpaper setting intentionally starts no process.

## How it works

`Meta.WaylandClient` identifies the extension's own window. A non-reactive
`Clutter.Clone` per monitor presents that window in `Main.layoutManager._backgroundGroup`, while
the original stays minimized and hidden from the window list. The extension owns
the renderer processes and sends SIGSTOP/SIGCONT for pause, and SIGTERM on disable.
If a hidden client does not exit within five seconds, it is forcibly stopped.
Unexpected renderer exits are retried after five seconds; active lock backgrounds
are rebound and desktop playback retains its role after unlocking.
It resumes a stopped child before terminating it. Unlocking rebuilds every desktop clone above the static backgrounds. Monitor changes restart playback
using the updated monitor layout. The source window uses the primary monitor
size; its image is scaled to fit each monitor.

Lock backgrounds clone only the extension's own native Wayland surfaces into
`UnlockDialog`'s per-monitor background widgets. Authentication and input handling
stay with GNOME. The hook is restored on disable and the original backgrounds return.

The desktop background group and lock background methods are private GNOME APIs. Newer Shell versions need a separate
port and testing; changing the metadata version list alone is not sufficient.

## Verification

Run the isolated integration test with an installed engine and a short looping
video project (requires `dbus-run-session`, GNOME Shell 42, Python GI/GTK3 and Pillow):

```sh
python3 packaging/gnome/smoke-test.py \
  --engine /absolute/path/to/linux-wallpaperengine \
  --wallpaper /absolute/path/to/video-project \
  --assets-dir /absolute/path/to/assets \
  --output build/gnome-test
```

The test starts a separate headless GNOME session. It checks changing background
frames, pause/resume, fullscreen pausing, settings-driven restart, and cleanup on
disable. This is a functional test, not a desktop performance benchmark. Add `--monitors 2` to check
both backgrounds, continued playback with one fullscreen monitor, and pause when
both monitors are covered. Add `--lock-wallpaper /absolute/path/to/another-video-project`
to also check different animated lock backgrounds, the password prompt remaining
visible, manual pause, unlock cleanup, repeated activation, and disable/re-enable
while the shield is present. The isolated test activates the real GNOME screen
shield with a test-only probe; it does not validate password authentication.
Regular desktop performance and mixed-scale multi-monitor behavior still need testing.

Use `--lock-lifecycle` with `--monitors 2` to exercise the real lock entry with a
right-hand primary monitor, repeated unlocks, clear video, keep-awake requests,
and static-background restacking. The probe isolates logind lock hints from the
host session. Virtual outputs cannot verify physical monitor DPMS behavior.
