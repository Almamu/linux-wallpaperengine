#!/usr/bin/env python3
"""Rotate Steam Workshop wallpapers every ten minutes.

The renderer itself remains unchanged: this helper supervises one renderer
process at a time, rescans the Workshop directory before every selection, and
keeps the current wallpaper in a small state directory for manual control and
diagnostics.

Do not suspend the renderer with SIGSTOP: a suspended process can hold PipeWire
nodes and can fail to establish its Wayland layer during session startup.
"""

import json
import os
from pathlib import Path
import random
import re
import shutil
import signal
import subprocess
import time

INTERVAL = 10 * 60
WORKSHOP_APP_ID = "431960"
STATE = Path.home() / ".local/state/linux-wallpaperengine"
STATE.mkdir(parents=True, exist_ok=True)

running = True
advance = False
child = None


def stop(_signum, _frame):
    global running
    running = False


def next_wallpaper(_signum, _frame):
    global advance
    advance = True


signal.signal(signal.SIGTERM, stop)
signal.signal(signal.SIGINT, stop)
signal.signal(signal.SIGUSR1, next_wallpaper)


def save(name, data):
    temp = STATE / (name + ".tmp")
    temp.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
    temp.replace(STATE / name)


def load(name, default):
    try:
        return json.loads((STATE / name).read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return default


def steamapps_roots():
    home = Path.home()
    data_home = Path(os.environ.get("XDG_DATA_HOME", home / ".local/share"))
    candidates = (
        data_home / "Steam/steamapps",
        home / ".steam/steam/steamapps",
        home / ".var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps",
        home / "snap/steam/common/.local/share/Steam/steamapps",
    )
    roots = []
    for root in candidates:
        if root not in roots:
            roots.append(root)
    for steamapps in tuple(roots):
        for listing in (
            steamapps / "libraryfolders.vdf",
            steamapps.parent / "config/libraryfolders.vdf",
        ):
            try:
                contents = listing.read_text(encoding="utf-8-sig")
            except (OSError, UnicodeError):
                continue
            entries = re.finditer(r'^\s*"path"\s*"((?:\\.|[^"])*)"', contents, re.MULTILINE)
            legacy_entries = re.finditer(r'^\s*"\d+"\s*"((?:\\.|[^"])*)"', contents, re.MULTILINE)
            for match in (*entries, *legacy_entries):
                try:
                    library_path = Path(json.loads(f'"{match.group(1)}"')).expanduser()
                except (ValueError, OSError):
                    continue
                if not library_path.is_absolute():
                    continue
                library = library_path if library_path.name == "steamapps" else library_path / "steamapps"
                if library not in roots:
                    roots.append(library)
    return roots


def renderer_path():
    candidates = (
        os.environ.get("LINUX_WALLPAPERENGINE_RENDERER_PATH"),
        shutil.which("linux-wallpaperengine"),
        str(Path.home() / ".local/bin/linux-wallpaperengine"),
        str(Path.home() / ".local/opt/linux-wallpaperengine/linux-wallpaperengine"),
        "/usr/local/bin/linux-wallpaperengine",
        "/usr/bin/linux-wallpaperengine",
    )
    for candidate in candidates:
        if candidate and os.path.isfile(candidate) and os.access(candidate, os.X_OK):
            return candidate
    raise FileNotFoundError(
        "Could not find linux-wallpaperengine; set LINUX_WALLPAPERENGINE_RENDERER_PATH or add it to PATH."
    )


def installations():
    roots = steamapps_roots()
    fallback_assets = next(
        (
            root / "common/wallpaper_engine/assets"
            for root in roots
            if (root / "common/wallpaper_engine/assets").is_dir()
        ),
        None,
    )
    for steamapps in roots:
        workshop = steamapps / "workshop/content" / WORKSHOP_APP_ID
        if workshop.is_dir():
            local_assets = steamapps / "common/wallpaper_engine/assets"
            assets = local_assets if local_assets.is_dir() else (fallback_assets or local_assets)
            yield workshop, assets


def catalog():
    result = {}
    for workshop, assets in installations():
        for project in sorted(workshop.glob("*/project.json")):
            try:
                data = json.loads(project.read_text(encoding="utf-8-sig"))
            except (OSError, ValueError):
                continue
            kind = str(data.get("type", "")).lower()
            # The installed CEF web renderer is not used by this rotation
            # helper; scene and video projects are supported by the binary.
            if kind in {"scene", "video"}:
                result[project.parent.name] = {
                    "title": data.get("title", project.parent.name),
                    "type": kind,
                    "path": str(project.parent),
                    "assets": str(assets),
                }
    return result


def _kscreen_outputs():
    if not shutil.which("kscreen-doctor"):
        return []
    try:
        result = subprocess.run(
            ["kscreen-doctor", "-j"],
            capture_output=True,
            text=True,
            timeout=5,
            check=True,
        )
        data = json.loads(result.stdout)
    except (OSError, subprocess.SubprocessError, ValueError):
        return []
    if not isinstance(data, dict):
        return []
    return list(dict.fromkeys([
        output["name"]
        for output in data.get("outputs", [])
        if isinstance(output, dict) and output.get("enabled") and output.get("connected")
        and isinstance(output.get("name"), str)
    ]))


def _xrandr_outputs():
    if not shutil.which("xrandr"):
        return []
    try:
        result = subprocess.run(
            ["xrandr", "--query"],
            capture_output=True,
            text=True,
            timeout=5,
            check=True,
        )
    except (OSError, subprocess.SubprocessError):
        return []
    connected = []
    active = []
    for line in result.stdout.splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[1] == "connected":
            connected.append(parts[0])
            if re.search(r"\b\d+x\d+\+-?\d+\+-?\d+\b", line):
                active.append(parts[0])
    return list(dict.fromkeys(active or connected))


def outputs():
    names = _kscreen_outputs()
    if names:
        return names
    session_type = os.environ.get("XDG_SESSION_TYPE")
    if session_type == "wayland" or (
        session_type != "x11"
        and (not os.environ.get("DISPLAY") or bool(os.environ.get("WAYLAND_DISPLAY")))
    ):
        return []
    return _xrandr_outputs()


def terminate():
    global child
    if child is None:
        return
    try:
        os.killpg(child.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    try:
        child.wait(timeout=5)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(child.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        child.wait()
    child = None


previous = load("current.json", {}).get("id")
queue = load("queue.json", [])
failed = load("failed.json", {})

try:
    while running:
        items = catalog()
        now = time.time()
        eligible = [item for item in items if now - failed.get(item, 0) > 86400]
        queue = [item for item in queue if item in eligible]
        if not queue:
            queue = eligible.copy()
            random.shuffle(queue)
            if len(queue) > 1 and queue[0] == previous:
                queue[0], queue[1] = queue[1], queue[0]
        if not queue:
            print("No supported Workshop wallpapers found; retrying in 60 seconds.", flush=True)
            for _ in range(60):
                if not running:
                    break
                time.sleep(1)
            continue

        monitors = outputs()
        if not monitors:
            time.sleep(5)
            continue

        selected = queue.pop(0)
        selected_data = items[selected]
        save("queue.json", queue)
        cmd = [
            renderer_path(),
            "--silent",
            # Avoid waiting for PulseAudio when PULSE_SERVER is disabled below.
            "--noautomute",
            "--no-audio-processing",
            "--disable-mouse",
            "--fps",
            "30",
            "--layer",
            "bottom",
            "--no-fullscreen-pause",
        ]
        if Path(selected_data["assets"]).is_dir():
            cmd += ["--assets-dir", selected_data["assets"]]
        for monitor in monitors:
            cmd += [
                "--screen-root",
                monitor,
                "--bg",
                selected_data["path"],
                "--scaling",
                "fill",
            ]

        advance = False
        started = time.monotonic()
        wp_env = dict(
            os.environ,
            SDL_AUDIODRIVER="dummy",
            PULSE_SERVER="none",
            PIPEWIRE_REMOTE="none",
        )
        child = subprocess.Popen(cmd, start_new_session=True, env=wp_env)
        started_at = time.time()
        save(
            "current.json",
            dict(
                selected_data,
                id=selected,
                pid=child.pid,
                started_at=started_at,
                interval_seconds=INTERVAL,
                next_change_at=started_at + INTERVAL,
                screens=monitors,
            ),
        )
        print(
            f"Wallpaper: {selected} — {selected_data['title']}; "
            f"next change in {INTERVAL}s",
            flush=True,
        )
        while (
            running
            and not advance
            and child.poll() is None
            and time.monotonic() - started < INTERVAL
        ):
            time.sleep(0.2)

        crashed = child.poll() is not None
        if crashed:
            print(
                f"Wallpaper {selected} exited ({child.returncode}); skipping for 24h.",
                flush=True,
            )
            failed[selected] = time.time()
            save("failed.json", failed)
        terminate()
        previous = selected
        if crashed and running:
            time.sleep(2)
finally:
    terminate()
