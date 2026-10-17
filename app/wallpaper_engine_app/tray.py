"""Optional tray helper kept in a GTK 3 subprocess.

The control panel uses GTK 4. Keeping the status icon in a separate process
avoids loading GTK 3 and GTK 4 introspection namespaces into one process.

Either AyatanaAppIndicator3 or the legacy AppIndicator3 namespace satisfies
the helper; only a system with neither reports an error.
"""

from __future__ import annotations

import argparse
import importlib
import os
from pathlib import Path
import secrets
import socket
import subprocess
import sys
from threading import Event, Thread
from types import ModuleType
from typing import Callable

from gi.repository import GLib

from . import model
from .i18n import tr


class TrayBridge:
    """Own a small GTK 3 tray helper and forward its actions to GTK 4."""

    def __init__(self, on_show: Callable[[], None], on_quit: Callable[[], None]):
        self._on_show = on_show
        self._on_quit = on_quit
        self._ready = Event()
        self._error: str | None = None
        self._closed = Event()
        self._server: socket.socket | None = None
        self._thread: Thread | None = None
        self._process: subprocess.Popen[bytes] | None = None
        self._socket_path: Path | None = None

    @property
    def running(self) -> bool:
        return self._process is not None and self._process.poll() is None

    def start(
        self, *, open_label: str, quit_label: str, icon_file: str | None = None
    ) -> None:
        if self.running:
            return
        self.stop()
        self._ready.clear()
        self._closed.clear()
        self._error = None
        runtime_env = os.environ.get("XDG_RUNTIME_DIR")
        socket_dir = (
            Path(runtime_env) / "linux-wallpaperengine"
            if runtime_env
            else model.STATE_DIR / "tray"
        )
        socket_dir.mkdir(parents=True, exist_ok=True, mode=0o700)
        try:
            os.chmod(socket_dir, 0o700)
        except OSError:
            pass
        self._socket_path = socket_dir / (
            f"lwe-tray-{os.getpid()}-{secrets.token_hex(4)}.sock"
        )
        server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            server.bind(str(self._socket_path))
            os.chmod(self._socket_path, 0o600)
            server.listen(4)
            server.settimeout(0.25)
        except OSError:
            server.close()
            try:
                self._socket_path.unlink()
            except FileNotFoundError:
                pass
            self._socket_path = None
            raise
        self._server = server

        self._thread = Thread(target=self._listen, name="wallpaper-tray-bridge", daemon=True)
        self._thread.start()
        arguments = [
            sys.executable, "-m", "wallpaper_engine_app.tray", "--helper",
            "--socket", str(self._socket_path), "--open-label", open_label,
            "--quit-label", quit_label,
        ]
        icon_file = icon_file or os.environ.get("LINUX_WALLPAPERENGINE_ICON_FILE")
        if icon_file:
            arguments.extend(("--icon-file", icon_file))
        try:
            self._process = subprocess.Popen(
                arguments, stdin=subprocess.DEVNULL,
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            )
        except OSError:
            self.stop()
            raise

        if not self._ready.wait(3.0):
            self.stop()
            if self._error:
                raise RuntimeError(self._error)
            raise RuntimeError(tr("A bandeja requer GTK 3 e Ayatana AppIndicator."))
        if self._error:
            details = self._error
            self.stop()
            raise RuntimeError(details)

    def _listen(self) -> None:
        server = self._server
        while server is not None and not self._closed.is_set():
            try:
                connection, _address = server.accept()
            except TimeoutError:
                continue
            except OSError:
                return
            with connection:
                try:
                    message = connection.recv(4096).decode("utf-8", errors="replace").strip()
                except OSError:
                    continue
            if message == "ready":
                self._ready.set()
            elif message.startswith("error:"):
                self._error = message.partition(":")[2].strip()
                self._ready.set()
            elif message == "show":
                GLib.idle_add(self._on_show)
            elif message == "quit":
                GLib.idle_add(self._on_quit)

    def stop(self) -> None:
        self._closed.set()
        server, self._server = self._server, None
        if server is not None:
            server.close()
        thread, self._thread = self._thread, None
        if thread is not None and thread.is_alive():
            thread.join(timeout=1.0)
        process, self._process = self._process, None
        if process is not None and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=1.0)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        if self._socket_path is not None:
            try:
                self._socket_path.unlink()
            except FileNotFoundError:
                pass
            self._socket_path = None


def _notify_parent(path: str, message: str) -> None:
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
        client.settimeout(1.0)
        client.connect(path)
        client.sendall(message.encode("utf-8"))


def _require_indicator() -> ModuleType:
    """Return the first available AppIndicator introspection namespace.

    Distributions ship either AyatanaAppIndicator3 (ayatana) or the legacy
    AppIndicator3; both expose the same Indicator API used below.
    """
    import gi

    for namespace in ("AyatanaAppIndicator3", "AppIndicator3"):
        try:
            gi.require_version(namespace, "0.1")
            return importlib.import_module(f"gi.repository.{namespace}")
        except Exception:
            # Missing typelib or introspection module: try the next namespace.
            continue
    raise RuntimeError(
        tr(
            "A bandeja precisa do Ayatana AppIndicator ou do AppIndicator3. "
            "Instale: Debian/Ubuntu gir1.2-ayatanaappindicator3-0.1, "
            "Fedora libayatana-appindicator-gtk3, Arch libayatana-appindicator."
        )
    )


def _helper_main(arguments: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--helper", action="store_true")
    parser.add_argument("--socket", required=True)
    parser.add_argument("--open-label", required=True)
    parser.add_argument("--quit-label", required=True)
    parser.add_argument("--icon-file")
    options = parser.parse_args(arguments)

    try:
        import gi

        gi.require_version("Gtk", "3.0")
        from gi.repository import Gtk

        AppIndicator = _require_indicator()
        icon_theme_dir: Path | None = None
        if options.icon_file:
            icon = Path(options.icon_file)
            if icon.is_file():
                icon_theme_dir = (
                    icon.parents[3]
                    if icon.parent.name == "apps" and len(icon.parents) > 3
                    else icon.parent
                )
                Gtk.IconTheme.get_default().append_search_path(str(icon_theme_dir))
        indicator = AppIndicator.Indicator.new(
            "linux-wallpaperengine-app",
            "linux-wallpaperengine-app",
            AppIndicator.IndicatorCategory.APPLICATION_STATUS,
        )
        indicator.set_status(AppIndicator.IndicatorStatus.ACTIVE)
        if icon_theme_dir is not None:
            indicator.set_icon_theme_path(str(icon_theme_dir))

        menu = Gtk.Menu()
        show_item = Gtk.MenuItem(label=options.open_label)
        show_item.connect("activate", lambda *_: _notify_parent(options.socket, "show"))
        menu.append(show_item)
        quit_item = Gtk.MenuItem(label=options.quit_label)
        quit_item.connect("activate", lambda *_: _notify_parent(options.socket, "quit"))
        menu.append(quit_item)
        menu.show_all()
        indicator.set_menu(menu)
        _notify_parent(options.socket, "ready")

        def check_parent() -> bool:
            try:
                _notify_parent(options.socket, "ping")
            except OSError:
                Gtk.main_quit()
                return False
            return True

        GLib.timeout_add_seconds(3, check_parent)
        Gtk.main()
        return 0
    except Exception as error:
        try:
            _notify_parent(options.socket, f"error:{error}")
        except OSError:
            pass
        print(error, file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(_helper_main(sys.argv[1:]))
