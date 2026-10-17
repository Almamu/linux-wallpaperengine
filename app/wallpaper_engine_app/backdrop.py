"""Window-manager backdrop effects with a safe opaque fallback."""

from __future__ import annotations

import ctypes
import os


def enable_backdrop_blur(window: object) -> bool:
    """Ask KWin to blur the complete X11/XWayland surface behind this window."""
    desktop = " ".join((
        os.environ.get("XDG_CURRENT_DESKTOP", ""),
        os.environ.get("XDG_SESSION_DESKTOP", ""),
    )).casefold()
    if "kde" not in desktop and "plasma" not in desktop:
        return False

    try:
        import gi

        gi.require_version("GdkX11", "4.0")
        from gi.repository import GdkX11

        display = window.get_display()
        surface = window.get_surface()
        if not isinstance(display, GdkX11.X11Display) or not hasattr(surface, "get_xid"):
            return False

        xdisplay = ctypes.c_void_p(hash(display.get_xdisplay()))
        xid = int(surface.get_xid())
        xlib = ctypes.CDLL("libX11.so.6")
        xlib.XInternAtom.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
        xlib.XInternAtom.restype = ctypes.c_ulong
        xlib.XChangeProperty.argtypes = [
            ctypes.c_void_p, ctypes.c_ulong, ctypes.c_ulong, ctypes.c_ulong,
            ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_ubyte), ctypes.c_int,
        ]
        xlib.XChangeProperty.restype = ctypes.c_int
        xlib.XFlush.argtypes = [ctypes.c_void_p]
        xlib.XFlush.restype = ctypes.c_int

        blur_atom = xlib.XInternAtom(xdisplay, b"_KDE_NET_WM_BLUR_BEHIND_REGION", 0)
        cardinal_atom = xlib.XInternAtom(xdisplay, b"CARDINAL", 0)
        # KWin clips this generously sized local region to the actual surface.
        region = (ctypes.c_ulong * 4)(0, 0, 32767, 32767)
        result = xlib.XChangeProperty(
            xdisplay, xid, blur_atom, cardinal_atom, 32, 0,
            ctypes.cast(region, ctypes.POINTER(ctypes.c_ubyte)), 4,
        )
        xlib.XFlush(xdisplay)
        return result != 0
    except (ImportError, AttributeError, OSError, TypeError, ValueError):
        # GTK remains usable without the optional KWin X11 blur hint.
        return False
