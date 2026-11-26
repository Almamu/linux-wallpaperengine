#!/usr/bin/env python3
"""Private native preparation and X11 helpers; this module never launches an app."""
import ctypes
import json
from pathlib import Path
import re
import subprocess


STEAM = Path.home() / '.local/share/Steam'
APP = STEAM / 'steamapps/common/wallpaper_engine'
COMPAT = STEAM / 'steamapps/compatdata/431960'
PROTON = STEAM / 'steamapps/common/Proton - Experimental'
TITLE = 'LWE Private Native Probe'


def win(path):
    return 'Z:' + str(path).replace('/', '\\')


def validate_display(private, host):
    display = re.fullmatch(r':(\d+)(?:\.\d+)?', private)
    host_display = re.search(r':(\d+)(?:\.\d+)?$', host)
    if not display or host_display and int(display[1]) == int(host_display[1]):
        raise ValueError('Refusing native launch without a distinct private local X display')


def prepare(root, width, height):
    root = Path(root).resolve()
    if root == Path('/tmp') or not root.is_relative_to('/tmp'):
        raise ValueError('Use a new private directory below /tmp')
    if root.exists():
        raise FileExistsError(root)
    if not (64 <= width <= 7680 and 64 <= height <= 4320):
        raise ValueError('Invalid native capture dimensions')
    app, compat = APP.resolve(strict=True), COMPAT.resolve(strict=True)
    if root.is_relative_to(app) or root.is_relative_to(compat):
        raise ValueError('Private root must be outside the application and prefix inputs')
    system32_path = Path('pfx/drive_c/windows/system32')
    for path in (system32_path, *system32_path.parents):
        if (compat / path).is_symlink():
            raise ValueError(f'Private DLL directory must not traverse a prefix symlink: {path}')
    dxvk = PROTON / 'files/lib/wine/dxvk/x86_64-windows'
    for path in (app / 'wallpaper64.exe', app / 'config.json', compat / 'pfx/user.reg',
                 PROTON / 'proton', dxvk / 'd3d11.dll', dxvk / 'dxgi.dll'):
        if not path.is_file():
            raise FileNotFoundError(f'Missing installed native capture input: {path}')
    for path in app.rglob('*'):
        if path.is_symlink():
            raise ValueError(f'Application contains a symlink; resolve its copy scope first: {path}')
    root.mkdir(mode=0o700, parents=True)
    for source, name in ((app, 'app'), (compat, 'compat')):
        subprocess.run(['cp', '-a', '--reflink=auto', str(source), str(root / name)], check=True)
    (root / 'app/bin/wallpaperui.exe').unlink(missing_ok=True)
    removed = []
    # Keep internal prefix links and the installed Proton runtime. Z: supplies
    # absolute private paths; it is not a filesystem security boundary.
    for path in (root / 'compat').rglob('*'):
        if not path.is_symlink():
            continue
        target = path.resolve()
        if (path == root / 'compat/pfx/dosdevices/z:' and target == Path('/')
                or target.is_relative_to(root) or target.is_relative_to(PROTON.resolve())):
            continue
        removed.append(dict(path=str(path.relative_to(root)), target=str(target)))
        path.unlink()
    (root / 'removed-symlinks.json').write_text(json.dumps(removed, indent=2) + '\n')
    system32 = root / 'compat' / system32_path
    system32.mkdir(parents=True, exist_ok=True)
    for name in ('d3d11.dll', 'dxgi.dll'):
        destination = system32 / name
        destination.unlink(missing_ok=True)  # Never overwrite a retained runtime symlink.
        subprocess.run(['cp', '--reflink=auto', '--dereference', str(dxvk / name), str(destination)], check=True)
    config_path = root / 'app/config.json'
    original = config_path.read_bytes()
    (root / 'config-before.json').write_bytes(original)
    config = json.loads(original)
    config['?installdirectory'] = win(root / 'app')
    config_path.write_text(json.dumps(config, indent=2) + '\n')


class X11:
    def __init__(self):
        self.lib = ctypes.CDLL('libX11.so.6')
        x = self.lib
        # Wine removes splash/utility windows during enumeration. A stale XID
        # must not invoke Xlib's default process-exiting error handler.
        self.error_handler = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p)(
            lambda display, event: 0)
        x.XSetErrorHandler.argtypes = [ctypes.c_void_p]
        x.XSetErrorHandler.restype = ctypes.c_void_p
        self.previous_error_handler = x.XSetErrorHandler(self.error_handler)
        x.XOpenDisplay.argtypes = [ctypes.c_char_p]
        x.XOpenDisplay.restype = ctypes.c_void_p
        x.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
        x.XDefaultRootWindow.restype = ctypes.c_ulong
        x.XQueryTree.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(ctypes.c_ulong),
            ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.POINTER(ctypes.c_ulong)),
            ctypes.POINTER(ctypes.c_uint)]
        x.XFetchName.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(ctypes.c_void_p)]
        x.XGetGeometry.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(ctypes.c_ulong),
            ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int),
            *[ctypes.POINTER(ctypes.c_uint)] * 4]
        x.XFree.argtypes = [ctypes.c_void_p]
        x.XCloseDisplay.argtypes = [ctypes.c_void_p]
        self.display = x.XOpenDisplay(None)
        if not self.display:
            x.XSetErrorHandler(self.previous_error_handler)
            raise RuntimeError('Private X display unavailable')
        self.root = x.XDefaultRootWindow(self.display)

    def named(self, window=None, depth=0):
        window = self.root if window is None else window
        title = ctypes.c_void_p()
        self.lib.XFetchName(self.display, window, ctypes.byref(title))
        name = ctypes.string_at(title).decode(errors='replace') if title.value else ''
        if title.value:
            self.lib.XFree(title)
        if name == TITLE:
            yield hex(window)
        if depth >= 4:
            return
        root, parent, count = ctypes.c_ulong(), ctypes.c_ulong(), ctypes.c_uint()
        data = ctypes.POINTER(ctypes.c_ulong)()
        self.lib.XQueryTree(self.display, window, ctypes.byref(root), ctypes.byref(parent),
                           ctypes.byref(data), ctypes.byref(count))
        children = [data[i] for i in range(count.value)]
        if data:
            self.lib.XFree(data)
        for child in children:
            yield from self.named(child, depth + 1)

    def size(self, xid):
        root, x, y = ctypes.c_ulong(), ctypes.c_int(), ctypes.c_int()
        width, height, border, depth = [ctypes.c_uint() for _ in range(4)]
        ok = self.lib.XGetGeometry(self.display, int(xid, 16), ctypes.byref(root), ctypes.byref(x),
            ctypes.byref(y), ctypes.byref(width), ctypes.byref(height), ctypes.byref(border),
            ctypes.byref(depth))
        if not ok:
            raise RuntimeError('Private XID geometry unavailable')
        return [width.value, height.value]

    def close(self):
        self.lib.XCloseDisplay(self.display)
        self.lib.XSetErrorHandler(self.previous_error_handler)
