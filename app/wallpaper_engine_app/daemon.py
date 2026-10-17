"""Single-renderer supervisor and local JSON-line control socket."""

from __future__ import annotations

import errno
import json
import os
from pathlib import Path
import random
import shutil
import signal
import socket
import stat
import subprocess
import time
from typing import Any

from . import model
from .i18n import set_language, tr


_MAX_REQUEST = 1024 * 1024
_SCAN_SECONDS = 10.0
_CRASH_COOLDOWN = 24 * 60 * 60


class WallpaperDaemon:
    def __init__(self) -> None:
        self.config = model.load_config()
        set_language(self.config.get("language", "auto"))
        self.catalog: dict[str, dict[str, Any]] = {}
        self.outputs: list[str] = []
        self.active = True
        self.alive = True
        self.child: subprocess.Popen | None = None
        self.child_started = 0.0
        self.child_group: int | None = None
        self.terminating = False
        self.terminate_deadline = 0.0
        self.current_id: str | None = None
        self.screens: dict[str, str] = {}
        self.pending_id: str | None = None
        self.skip_requested = False
        self.next_change_mono: float | None = None
        self.next_change_at: float | None = None
        self.retry_at = 0.0
        self.next_scan = 0.0
        self.empty_output_scans = 0
        self.queue: list[str] = []
        self.failed: dict[str, float] = {}
        self.crash_streak = 0
        self.error: str | None = None
        self.sock: socket.socket | None = None
        self.socket_inode: int | None = None
        self._last_saved: dict[str, Any] | None = None

    def _status(self) -> dict[str, Any]:
        return {
            "running": self.active,
            "renderer_running": self.child is not None and self.child.poll() is None and not self.terminating,
            "current_id": self.current_id,
            "current_title": self.catalog.get(self.current_id or "", {}).get("title"),
            "screens": list(self.outputs),
            "screen_wallpapers": dict(self.screens),
            "outputs": list(self.outputs),
            "next_change_at": self.next_change_at,
            "pending_id": self.pending_id,
            "error": self.error,
            "catalog_count": len(self.catalog),
            "daemon_pid": os.getpid(),
            "renderer_pid": self.child.pid if self.child is not None and self.child.poll() is None else None,
            "config": self.config,
        }

    def _publish(self) -> None:
        status = self._status()
        if status != self._last_saved:
            model.save_status(dict(status, updated_at=time.time()))
            self._last_saved = status

    def _refresh(self, *, force: bool = False) -> None:
        if not force and time.monotonic() < self.next_scan:
            return
        self.next_scan = time.monotonic() + _SCAN_SECONDS
        new_catalog = model.scan_catalog()
        new_outputs = model.detect_outputs()
        if new_outputs:
            self.empty_output_scans = 0
        else:
            self.empty_output_scans += 1
            # KScreen can return no displays briefly during a Plasma login or
            # compositor restart. Keep the current renderer until the absence
            # persists across several scans.
            if self.outputs and self.empty_output_scans < 3:
                new_outputs = self.outputs
        old_outputs = self.outputs
        self.catalog = new_catalog
        self.outputs = new_outputs
        if self.child is not None and not self.terminating:
            if self.current_id and self.current_id not in new_catalog:
                self._request_switch(None)
            elif old_outputs != new_outputs:
                self._request_switch(self.current_id)

    def _available(self, wallpaper_id: str | None, *, now: float | None = None) -> bool:
        if wallpaper_id is None or wallpaper_id not in self.catalog:
            return False
        current = time.time() if now is None else now
        return current - self.failed.get(wallpaper_id, 0) >= _CRASH_COOLDOWN

    def _eligible(self) -> list[str]:
        now = time.time()
        playlist_name = self.config["active_playlist"]
        if playlist_name is not None:
            return [
                wallpaper_id for wallpaper_id in self.config["playlists"][playlist_name]
                if self._available(wallpaper_id, now=now)
            ]
        favorites = set(self.config["favorites"])
        return [
            wallpaper_id for wallpaper_id in sorted(self.catalog, key=int)
            if (not self.config["only_favorites"] or wallpaper_id in favorites)
            and self._available(wallpaper_id, now=now)
        ]

    def _next_id(self) -> str | None:
        eligible = self._eligible()
        if not eligible:
            return None
        if not self.config["shuffle"]:
            if self.current_id in eligible:
                return eligible[(eligible.index(self.current_id) + 1) % len(eligible)]
            return eligible[0]
        self.queue = [wallpaper_id for wallpaper_id in self.queue if wallpaper_id in eligible]
        if not self.queue:
            self.queue = eligible.copy()
            random.shuffle(self.queue)
            if len(self.queue) > 1 and self.queue[0] == self.current_id:
                self.queue[0], self.queue[1] = self.queue[1], self.queue[0]
        return self.queue.pop(0)

    def _first_id(self) -> str | None:
        selected = self.config["selected_id"]
        if not self.config["rotation_enabled"] and self._available(selected):
            return selected
        eligible = self._eligible()
        if self.config["active_playlist"] is not None:
            if self.current_id in eligible:
                return self.current_id
            return self._next_id() if self.config["shuffle"] else (eligible[0] if eligible else None)
        if selected in eligible:
            return selected
        if self.current_id in eligible:
            return self.current_id
        return self._next_id()

    def _resolve_renderer(self) -> str:
        bundled = os.environ.get("LINUX_WALLPAPERENGINE_RENDERER_PATH")
        if bundled:
            path = bundled
        else:
            configured = self.config["renderer_path"]
            path = shutil.which("linux-wallpaperengine") if configured == "auto" else configured
            if not path and configured == "auto":
                for candidate in (
                    Path.home() / ".local/bin/linux-wallpaperengine",
                    Path.home() / ".local/opt/linux-wallpaperengine/linux-wallpaperengine",
                    Path("/usr/local/bin/linux-wallpaperengine"),
                    Path("/usr/bin/linux-wallpaperengine"),
                ):
                    if candidate.is_file() and os.access(candidate, os.X_OK):
                        path = str(candidate)
                        break
        if not path or not Path(path).is_file() or not os.access(path, os.X_OK):
            raise RuntimeError(tr("Renderizador linux-wallpaperengine não encontrado ou sem permissão de execução."))
        return path

    def _screen_wallpapers(self, wallpaper_id: str) -> dict[str, str]:
        if wallpaper_id not in self.catalog:
            raise ValueError(tr("Wallpaper selecionado não está mais instalado."))
        if not self.outputs:
            raise RuntimeError(tr("Nenhum monitor ativo detectado."))

        screens: dict[str, str] = {}
        for screen in self.outputs:
            assigned = self.config["screen_assignments"].get(screen)
            # An unavailable fixed wallpaper must not crash the renderer on
            # every display while its cooldown is active.
            screens[screen] = assigned if self._available(assigned) else wallpaper_id
        return screens

    def _renderer_environment(self) -> dict[str, str]:
        environment = os.environ.copy()
        bundled_libraries = environment.get("LINUX_WALLPAPERENGINE_LIBRARY_PATH")
        if bundled_libraries:
            existing_libraries = environment.get("LD_LIBRARY_PATH")
            environment["LD_LIBRARY_PATH"] = (
                f"{bundled_libraries}:{existing_libraries}" if existing_libraries else bundled_libraries
            )
        if self.config["mute"]:
            environment.update(
                SDL_AUDIODRIVER="dummy", PULSE_SERVER="none", PIPEWIRE_REMOTE="none"
            )
        return environment

    def _append_asset_directory(self, command: list[str], screens: dict[str, str]) -> None:
        assets = [self.catalog[selected]["assets"] for selected in screens.values()]
        existing_assets = [path for path in assets if Path(path).is_dir()]
        if existing_assets:
            # The renderer has one global assets directory for all screens.
            command.extend(("--assets-dir", existing_assets[0]))

    def _append_screen_options(self, command: list[str], screens: dict[str, str]) -> None:
        for screen, selected in screens.items():
            command.extend((
                "--screen-root", screen,
                "--bg", self.catalog[selected]["path"],
                "--scaling", self.config["scaling"],
            ))
            supported = self.catalog[selected].get("properties", {})
            overrides = self.config["wallpaper_properties"].get(selected, {})
            for name, value in overrides.items():
                if name in supported:
                    command.extend(("--screen-property", f"{name}={'true' if value else 'false'}"))

    def _build_command(self, wallpaper_id: str) -> tuple[list[str], dict[str, str], dict[str, str]]:
        screens = self._screen_wallpapers(wallpaper_id)
        renderer = self._resolve_renderer()
        command = [
            renderer, "--disable-mouse", "--fps", str(self.config["fps"]),
            "--layer", "bottom",
        ]
        environment = self._renderer_environment()
        if self.config["mute"]:
            command.extend(("--silent", "--noautomute", "--no-audio-processing"))
        self._append_asset_directory(command, screens)
        self._append_screen_options(command, screens)
        return command, environment, screens

    def _active_or_pending_id(self) -> str | None:
        return self.pending_id if self.pending_id is not None else self.current_id

    def _request_switch(self, wallpaper_id: str | None) -> None:
        self.pending_id = wallpaper_id
        self.next_change_mono = None
        self.next_change_at = None
        if self.child is not None:
            self._signal_child(signal.SIGTERM)
            self.terminating = True
            self.terminate_deadline = time.monotonic() + 3.0

    def _signal_child(self, sig: signal.Signals | None) -> None:
        if sig is None or self.child_group is None:
            return
        try:
            os.killpg(self.child_group, sig)
        except ProcessLookupError:
            pass

    def _reap_child(self) -> None:
        if self.child is None:
            return
        returncode = self.child.poll()
        if returncode is None:
            if self.terminating and time.monotonic() >= self.terminate_deadline:
                self._signal_child(signal.SIGKILL)
                self.terminate_deadline = time.monotonic() + 1.0
            return

        was_terminating = self.terminating
        old_id = self.current_id
        active_ids = set(self.screens.values())
        if not active_ids and old_id is not None:
            active_ids.add(old_id)
        # Catch helper processes that outlived the renderer's main process.
        self._signal_child(signal.SIGKILL)
        self.child = None
        self.child_group = None
        self.terminating = False
        self.screens = {}
        self.next_change_mono = None
        self.next_change_at = None
        if not was_terminating and self.active:
            failed_at = time.time()
            for wallpaper_id in active_ids:
                self.failed[wallpaper_id] = failed_at
            if time.monotonic() - self.child_started > 60:
                self.crash_streak = 0
            self.crash_streak += 1
            delay = min(10 * 2 ** min(self.crash_streak - 1, 6), 600)
            self.error = tr(
                "O renderizador encerrou (código {code}); nova tentativa em {delay} segundos.",
                code=returncode, delay=delay,
            )
            self.pending_id = self._next_id()
            self.retry_at = time.monotonic() + delay

    def _start_child(self, wallpaper_id: str) -> None:
        try:
            command, environment, screens = self._build_command(wallpaper_id)
            child = subprocess.Popen(command, start_new_session=True, env=environment)
        except (OSError, ValueError, RuntimeError) as exc:
            self.error = str(exc)
            self.retry_at = time.monotonic() + 10.0
            return
        self.child = child
        self.child_group = child.pid
        self.child_started = time.monotonic()
        self.current_id = wallpaper_id
        self.screens = screens
        self.pending_id = None
        self.error = None
        rotating = any(screen not in self.config["screen_assignments"] for screen in self.outputs)
        if self.config["rotation_enabled"] and rotating:
            interval = self.config["interval_minutes"] * 60
            self.next_change_mono = self.child_started + interval
            self.next_change_at = time.time() + interval
        else:
            self.next_change_mono = None
            self.next_change_at = None
        print(f"Wallpaper: {wallpaper_id} — {self.catalog[wallpaper_id]['title']}", flush=True)

    def _advance_rotation_if_due(self, now: float) -> None:
        if self.child is None or self.terminating or self.next_change_mono is None:
            return
        if now < self.next_change_mono:
            return
        selected = self._next_id()
        if selected and selected != self.current_id:
            self._request_switch(selected)
            return
        interval = self.config["interval_minutes"] * 60
        self.next_change_mono = now + interval
        self.next_change_at = time.time() + interval

    def _restore_fixed_assignments(self) -> None:
        if self.child is None or self.terminating:
            return
        for screen in self.outputs:
            assigned = self.config["screen_assignments"].get(screen)
            if (
                assigned in self.catalog
                and self.screens.get(screen) != assigned
                and self._available(assigned)
            ):
                # A fixed wallpaper that crashed is temporarily replaced
                # while it is cooling down. Rebuild the renderer once the
                # cooldown expires so the fixed assignment is restored.
                self._request_switch(self.current_id)
                return

    def _pending_wallpaper(self) -> str | None:
        pending_allowed = self._available(self.pending_id)
        if self.config["rotation_enabled"]:
            pending_allowed = pending_allowed and self.pending_id in self._eligible()
        selected = self.pending_id if pending_allowed else self._first_id()
        if selected is not None:
            return selected

        assignments = self.config["screen_assignments"]
        all_outputs_fixed = self.outputs and all(
            screen in assignments and self._available(assignments[screen])
            for screen in self.outputs
        )
        if all_outputs_fixed:
            return assignments[self.outputs[0]]
        return None

    def _set_selection_error(self, now: float) -> None:
        if self.config["active_playlist"] is not None:
            self.error = tr(
                "A playlist '{playlist}' não contém wallpapers instalados e disponíveis para rotação.",
                playlist=self.config["active_playlist"],
            )
        elif self.catalog:
            self.error = tr(
                "Nenhum wallpaper elegível encontrado. Confira as assinaturas da Steam e o filtro de favoritos."
            )
        else:
            self.error = tr("Nenhum wallpaper scene ou video do Workshop encontrado.")
        self.retry_at = now + _SCAN_SECONDS

    def _start_child_if_ready(self, now: float) -> None:
        if self.child is not None or now < self.retry_at:
            return
        selected = self._pending_wallpaper()
        if selected is None:
            self._set_selection_error(now)
        elif not self.outputs:
            self.error = tr("Nenhum monitor ativo detectado.")
            self.retry_at = now + _SCAN_SECONDS
        else:
            self._start_child(selected)

    def _tick(self) -> None:
        if self.skip_requested:
            self.skip_requested = False
            if self.active:
                selected = self._next_id()
                if selected is not None:
                    if not self.config["rotation_enabled"]:
                        self.config = model.save_config(dict(self.config, selected_id=selected))
                    self.retry_at = 0.0
                    self._request_switch(selected)
        self._reap_child()
        self._refresh()
        if not self.active:
            self._publish()
            return

        now = time.monotonic()
        self._advance_rotation_if_due(now)
        self._restore_fixed_assignments()
        self._start_child_if_ready(now)
        self._publish()

    def _validate_known_id(self, value: Any) -> str:
        wallpaper_id = model.normalize_id(value)
        if wallpaper_id not in self.catalog:
            raise ValueError(tr("Wallpaper {wallpaper_id} não está instalado.", wallpaper_id=wallpaper_id))
        return wallpaper_id

    def _validate_known_config(self, config: dict[str, Any]) -> None:
        """Reject newly introduced unknown IDs while tolerating stale saved ones."""
        old_favorites = set(self.config["favorites"])
        for wallpaper_id in set(config["favorites"]) - old_favorites:
            self._validate_known_id(wallpaper_id)
        selected = config["selected_id"]
        if selected is not None and selected != self.config["selected_id"]:
            self._validate_known_id(selected)
        for screen, wallpaper_id in config["screen_assignments"].items():
            if self.config["screen_assignments"].get(screen) != wallpaper_id:
                self._validate_known_id(wallpaper_id)
        old_properties = self.config["wallpaper_properties"]
        for wallpaper_id, properties in config["wallpaper_properties"].items():
            if properties == old_properties.get(wallpaper_id):
                continue
            self._validate_known_id(wallpaper_id)
            supported = self.catalog[wallpaper_id].get("properties", {})
            unknown = set(properties) - set(supported)
            if unknown:
                raise ValueError(tr(
                    "Opções desconhecidas para o wallpaper {wallpaper_id}: {names}.",
                    wallpaper_id=wallpaper_id, names=", ".join(sorted(unknown)),
                ))

    def _validate_request(self, message: dict[str, Any]) -> str:
        if not isinstance(message, dict):
            raise ValueError(tr("Solicitação inválida."))
        command = message.get("command")
        if not isinstance(command, str):
            raise ValueError(tr("Comando inválido."))
        expected = {
            "status": set(), "next": set(), "select": {"id"},
            "set": {"settings"}, "assign": {"screen", "id"},
            "start": set(), "stop": set(), "reload": set(), "shutdown": set(),
        }
        if command not in expected:
            raise ValueError(tr("Comando desconhecido: {command}.", command=command))
        supplied = set(message) - {"command"}
        if supplied != expected[command]:
            raise ValueError(tr("Argumentos inválidos para {command}.", command=command))
        return command

    def _command_status(self, _message: dict[str, Any]) -> None:
        return

    def _command_next(self, _message: dict[str, Any]) -> None:
        self._refresh(force=True)
        selected = self._next_id()
        if selected is None:
            if self.config["active_playlist"] is not None:
                raise ValueError(tr("A playlist ativa não contém wallpapers instalados e disponíveis."))
            raise ValueError(tr("Nenhum wallpaper elegível para avançar."))
        self.config = model.save_config(dict(self.config, selected_id=selected))
        self.active = True
        self.retry_at = 0.0
        self._request_switch(selected)

    def _command_select(self, message: dict[str, Any]) -> None:
        raw_id = model.normalize_id(message["id"])
        self._refresh(force=not self.catalog or not self.outputs or raw_id not in self.catalog)
        selected = self._validate_known_id(raw_id)
        self.failed.pop(selected, None)
        self.crash_streak = 0
        self.config = model.save_config(dict(
            self.config, selected_id=selected, rotation_enabled=False,
            active_playlist=None, screen_assignments={},
        ))
        self.queue = []
        self.active = True
        self.retry_at = 0.0
        self._request_switch(selected)

    def _prepare_settings_update(self, raw_settings: Any) -> dict[str, Any]:
        if not isinstance(raw_settings, dict):
            raise ValueError(tr("As configurações devem ser um objeto."))
        settings = dict(raw_settings)
        # Turning rotation off keeps the wallpaper currently on screen
        # across daemon and desktop session restarts.
        active_id = self._active_or_pending_id()
        if (
            settings.get("rotation_enabled") is False
            and self.config["rotation_enabled"]
            and "selected_id" not in settings
            and active_id in self.catalog
        ):
            settings["selected_id"] = active_id
        return model.validate_config(dict(self.config, **settings))

    def _requires_eligible_wallpaper(self, changed: set[str]) -> bool:
        target_id = self._active_or_pending_id()
        if not self.config["rotation_enabled"] or target_id in self._eligible():
            return False
        if "rotation_enabled" in changed or "active_playlist" in changed:
            return True
        if self.config["active_playlist"] is not None:
            return "playlists" in changed
        return bool(changed & {"favorites", "only_favorites"})

    def _update_rotation_deadline(self, changed: set[str]) -> None:
        if self.terminating or not changed & {"rotation_enabled", "interval_minutes"}:
            return
        rotating = any(screen not in self.config["screen_assignments"] for screen in self.outputs)
        if self.child is not None and self.config["rotation_enabled"] and rotating:
            interval = self.config["interval_minutes"] * 60
            self.next_change_mono = time.monotonic() + interval
            self.next_change_at = time.time() + interval
            return
        self.next_change_mono = None
        self.next_change_at = None

    def _apply_active_setting_changes(self, changed: set[str]) -> None:
        restart_keys = {
            "fps", "scaling", "mute", "renderer_path", "screen_assignments",
            "wallpaper_properties",
        }
        target_id = self._active_or_pending_id()
        selected_changed = "selected_id" in changed and self.config["selected_id"] != target_id
        if changed & restart_keys or selected_changed:
            selected = self.config["selected_id"] if selected_changed else target_id
            self.retry_at = 0.0
            self._request_switch(selected)
        elif self._requires_eligible_wallpaper(changed):
            self.retry_at = 0.0
            self._request_switch(self._next_id())
        elif self.child is None:
            self.retry_at = 0.0
        self._update_rotation_deadline(changed)

    def _command_set(self, message: dict[str, Any]) -> None:
        updated = self._prepare_settings_update(message["settings"])
        referenced_ids = (
            set(updated["favorites"])
            | set(updated["screen_assignments"].values())
            | set(updated["wallpaper_properties"])
        )
        if updated["selected_id"] is not None:
            referenced_ids.add(updated["selected_id"])
        needs_force = (
            not self.catalog
            or not self.outputs
            or bool(referenced_ids - set(self.catalog))
        )
        self._refresh(force=needs_force)
        self._validate_known_config(updated)
        previous = self.config
        self.config = model.save_config(updated)
        changed = {key for key in updated if updated[key] != previous[key]}

        if "selected_id" in changed and self.config["selected_id"] is not None:
            self.failed.pop(self.config["selected_id"], None)
            self.crash_streak = 0
        if "screen_assignments" in changed:
            for wallpaper_id in self.config["screen_assignments"].values():
                self.failed.pop(wallpaper_id, None)
            self.crash_streak = 0
        if "language" in changed:
            set_language(self.config["language"])
            self.error = None
            if self.child is None:
                self.retry_at = 0.0
        if changed & {"shuffle", "favorites", "only_favorites", "playlists", "active_playlist"}:
            self.queue = []
        if self.active and changed:
            self._apply_active_setting_changes(changed)

    def _command_assign(self, message: dict[str, Any]) -> None:
        screen = model.validate_screen(message["screen"])
        selected = message["id"]
        normalized_id = model.normalize_id(selected) if selected is not None else None
        needs_force = (
            not self.catalog
            or not self.outputs
            or screen not in self.outputs
            or (normalized_id is not None and normalized_id not in self.catalog)
        )
        self._refresh(force=needs_force)
        if screen not in self.outputs:
            raise ValueError(tr("Monitor {screen} não está ativo.", screen=screen))
        assignments = dict(self.config["screen_assignments"])
        if normalized_id is None:
            assignments.pop(screen, None)
        else:
            valid_id = self._validate_known_id(normalized_id)
            assignments[screen] = valid_id
            self.failed.pop(valid_id, None)
            self.crash_streak = 0
        self.config = model.save_config(dict(self.config, screen_assignments=assignments))
        if self.active:
            self.retry_at = 0.0
            self._request_switch(self._active_or_pending_id())

    def _command_start(self, _message: dict[str, Any]) -> None:
        self.active = True
        self.failed.clear()
        self.crash_streak = 0
        self.retry_at = 0.0
        if (self.child is None or self.terminating) and self.pending_id is None:
            self.pending_id = (
                self._first_id() if self.config["rotation_enabled"]
                else self.config["selected_id"] or self.current_id
            )

    def _command_stop(self, _message: dict[str, Any]) -> None:
        self.active = False
        self.pending_id = None
        self.next_change_mono = None
        self.next_change_at = None
        if self.child is not None:
            self._request_switch(None)

    def _command_shutdown(self, message: dict[str, Any]) -> None:
        self._command_stop(message)
        self.alive = False

    def _command_reload(self, _message: dict[str, Any]) -> None:
        self.failed.clear()
        self.crash_streak = 0
        self._refresh(force=True)
        if self.active:
            target = self._active_or_pending_id()
            selected = target if target in self.catalog else None
            self.retry_at = 0.0
            self._request_switch(selected)

    def _dispatch(self, message: dict[str, Any]) -> dict[str, Any]:
        command = self._validate_request(message)
        handlers = {
            "status": self._command_status,
            "next": self._command_next,
            "select": self._command_select,
            "set": self._command_set,
            "assign": self._command_assign,
            "start": self._command_start,
            "stop": self._command_stop,
            "shutdown": self._command_shutdown,
            "reload": self._command_reload,
        }
        handlers[command](message)
        self._tick()
        return self._status()

    def _handle_client(self, client: socket.socket) -> None:
        with client:
            client.settimeout(1.0)
            try:
                with client.makefile("rb") as reader:
                    raw = reader.readline(_MAX_REQUEST + 1)
                if not raw:
                    return
                if len(raw) > _MAX_REQUEST or not raw.endswith(b"\n"):
                    raise ValueError(tr("Solicitação grande demais ou incompleta."))
                message = json.loads(raw)
                status = self._dispatch(message)
                response = {"ok": True, "status": status}
            except (ValueError, UnicodeError, TypeError, OSError) as exc:
                response = {"ok": False, "error": str(exc)}
            try:
                client.sendall(json.dumps(response, ensure_ascii=False).encode("utf-8") + b"\n")
            except OSError:
                pass

    def _open_socket(self) -> socket.socket:
        path = model.SOCKET_FILE
        path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
        listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            try:
                listener.bind(str(path))
            except OSError as exc:
                if exc.errno != errno.EADDRINUSE:
                    raise
                try:
                    existing = path.lstat()
                except FileNotFoundError:
                    listener.bind(str(path))
                else:
                    if not stat.S_ISSOCK(existing.st_mode) or existing.st_uid != os.getuid():
                        raise RuntimeError(tr("Socket ocupado por outro arquivo: {path}", path=path)) from exc
                    try:
                        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as probe:
                            probe.settimeout(0.2)
                            probe.connect(str(path))
                    except OSError:
                        path.unlink(missing_ok=True)
                        listener.bind(str(path))
                    else:
                        raise RuntimeError(tr("O serviço de wallpapers já está em execução.")) from exc
            os.chmod(path, 0o600)
            self.socket_inode = path.stat().st_ino
            listener.listen(8)
            listener.settimeout(0.25)
            return listener
        except Exception:
            listener.close()
            raise

    def _close_socket(self) -> None:
        if self.sock is not None:
            self.sock.close()
            self.sock = None
        try:
            if model.SOCKET_FILE.lstat().st_ino == self.socket_inode:
                model.SOCKET_FILE.unlink()
        except OSError:
            pass

    def _shutdown_child(self) -> None:
        if self.child is None:
            return
        self._request_switch(None)
        deadline = time.monotonic() + 3.5
        while self.child is not None and time.monotonic() < deadline:
            self._reap_child()
            if self.child is not None:
                time.sleep(0.1)
        if self.child is not None:
            self._signal_child(signal.SIGKILL)
            try:
                self.child.wait(timeout=1)
            except subprocess.TimeoutExpired:
                pass
            self.child = None
            self.child_group = None

    def serve(self) -> None:
        self.sock = self._open_socket()
        signal.signal(signal.SIGTERM, lambda _sig, _frame: setattr(self, "alive", False))
        signal.signal(signal.SIGINT, lambda _sig, _frame: setattr(self, "alive", False))
        signal.signal(signal.SIGUSR1, lambda _sig, _frame: setattr(self, "skip_requested", True))
        try:
            self._refresh(force=True)
            while self.alive:
                self._tick()
                try:
                    client, _ = self.sock.accept()
                except socket.timeout:
                    continue
                except InterruptedError:
                    continue
                self._handle_client(client)
        finally:
            self.active = False
            self._shutdown_child()
            self._close_socket()
            self._publish()


def run() -> None:
    WallpaperDaemon().serve()
