# 🧪 Wayland & X11 Support

- **Wayland**: Works with compositors that support `wlr-layer-shell-unstable`.
- **X11**: Requires XRandr. Use `--screen-root <screen_name>` (as shown in `xrandr`).

> ⚠ For X11 users: Currently doesn't work if a compositor or desktop environment (e.g. GNOME, KDE, Nautilus) is drawing the background.

# Black screen when setting as screen's background
This can be caused by a few different things depending on your environment and setup.

## X11
Common symptom of a compositor drawing to the background which prevents Wallpaper Engine from being properly visible.
The only solution currently is disabling the compositor so Wallpaper Engine can properly draw on the screen

## NVIDIA
Some users have had issues with GLFW initialization and other OpenGL errors. These are generally something that's
worth reporting in the issues. Sometimes adding this variable when running Wallpaper Engine helps and/or solves
the issue:
```bash
__GL_THREADED_OPTIMIZATIONS=0 linux-wallpaperengine
```

We'll be looking at improving this in the future, but for now it can be a useful workaround.