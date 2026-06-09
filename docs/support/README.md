# Support list

## X11 support
X11 support requires XRandr. Use screen names returned by `xrandr` to specify the screen.
Compositors drawing to the screen background might conflict with the app.

- [Cinnamon](Cinnamon.md)
- [MATE](MATE.md)

## Wayland support
Works on compositors that support `wlr-layer-shell-unstable-v1`
- [Sway](Sway.md)
- [Hyprland](Hyprland.md)
- [COSMIC](Cosmic.md)

## Desktop environments support
- [GNOME](GNOME.md)
- [KDE](KDE.md)

## 🪲 Common issues
### Black screen when setting as screen's background
This can be caused by a few different things depending on your environment and setup.

### X11
Common symptom of a compositor drawing to the background which prevents Wallpaper Engine from being properly visible.
The only solution currently is disabling the compositor so Wallpaper Engine can properly draw on the screen

### NVIDIA
Some users have had issues with GLFW initialization and other OpenGL errors. These are generally something that's
worth reporting in the issues. Sometimes adding this variable when running Wallpaper Engine helps and/or solves
the issue:
```bash
__GL_THREADED_OPTIMIZATIONS=0 linux-wallpaperengine
```

We'll be looking at improving this in the future, but for now it can be a useful workaround.
