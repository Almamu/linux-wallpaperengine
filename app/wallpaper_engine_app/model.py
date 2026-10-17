"""Persistent settings and discovery shared by the GUI and daemon."""

from __future__ import annotations

import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import tempfile
from typing import Any
import unicodedata

from .i18n import tr


APP_NAME = "linux-wallpaperengine"
WORKSHOP_APP_ID = "431960"
LANGUAGE_CODES = ("auto", "pt_BR", "en", "de", "ru", "ja", "zh_CN", "es", "hi")


def _xdg_dir(variable: str, fallback: Path) -> Path:
    value = os.environ.get(variable)
    return Path(value).expanduser() if value else fallback


CONFIG_DIR = _xdg_dir("XDG_CONFIG_HOME", Path.home() / ".config") / APP_NAME
STATE_DIR = _xdg_dir("XDG_STATE_HOME", Path.home() / ".local/state") / APP_NAME
_runtime = os.environ.get("XDG_RUNTIME_DIR")
_socket_dir = Path(_runtime) if _runtime else STATE_DIR
SOCKET_FILE = _socket_dir / "linux-wallpaperengine-app.sock"
CONFIG_FILE = CONFIG_DIR / "app.json"
UI_PREFERENCES_FILE = CONFIG_DIR / "ui.json"
STATUS_FILE = STATE_DIR / "status.json"

DEFAULT_CONFIG: dict[str, Any] = {
    "rotation_enabled": True,
    "interval_minutes": 10,
    "shuffle": True,
    "favorites": [],
    "wallpaper_properties": {},
    "only_favorites": False,
    "playlists": {},
    "active_playlist": None,
    "selected_id": None,
    "screen_assignments": {},
    "fps": 30,
    "scaling": "fill",
    "mute": True,
    "renderer_path": "auto",
    "language": "auto",
    # Retained for migration from the remote branch; the GUI writes these to ui.json.
    "ui_hue": 24,
    "ui_intensity": 88,
}

_ID_PATTERN = re.compile(r"[0-9]{1,24}\Z")
_PROPERTY_PATTERN = re.compile(r"[A-Za-z_][A-Za-z0-9_.-]{0,127}\Z")
_SCALING = {"stretch", "fit", "fill", "default"}


def normalize_id(value: Any) -> str:
    """Return a Workshop ID, rejecting paths and arbitrary CLI values."""
    if isinstance(value, bool) or not isinstance(value, (int, str)):
        raise ValueError(tr("O ID do wallpaper deve ser numérico."))
    result = str(value)
    if not _ID_PATTERN.fullmatch(result):
        raise ValueError(tr("O ID do wallpaper deve ser numérico."))
    return result


def validate_screen(value: Any) -> str:
    if not isinstance(value, str) or not value or len(value) > 128:
        raise ValueError(tr("Nome de monitor inválido."))
    if any(ord(char) < 32 or char == "\x7f" for char in value):
        raise ValueError(tr("Nome de monitor inválido."))
    return value


def normalize_playlist_name(value: Any) -> str:
    if not isinstance(value, str):
        raise ValueError(tr("Nome de playlist inválido."))
    if any(unicodedata.category(char).startswith("C") for char in value):
        raise ValueError(tr("O nome da playlist não pode conter caracteres de controle."))
    name = unicodedata.normalize("NFC", value).strip()
    if not 1 <= len(name) <= 80:
        raise ValueError(tr("O nome da playlist deve ter de 1 a 80 caracteres."))
    return name


def normalize_language(value: Any) -> str:
    """Canonicalize a supported GUI language code for saved settings."""
    if not isinstance(value, str):
        raise ValueError(tr("Idioma inválido."))
    normalized = value.strip().replace("-", "_").casefold()
    canonical = {code.casefold(): code for code in LANGUAGE_CODES}
    if normalized not in canonical:
        raise ValueError(tr("Idioma inválido."))
    return canonical[normalized]


def _validate_scalar_config(result: dict[str, Any]) -> None:
    for key in ("rotation_enabled", "shuffle", "only_favorites", "mute"):
        if type(result[key]) is not bool:
            raise ValueError(tr("{key} deve ser verdadeiro ou falso.", key=key))

    numeric_ranges = (
        ("interval_minutes", 1, 1440, "O intervalo deve estar entre 1 e 1440 minutos."),
        ("fps", 1, 240, "FPS deve estar entre 1 e 240."),
        ("ui_hue", 0, 360, "Matiz da interface deve estar entre 0 e 360."),
        ("ui_intensity", 35, 100, "Intensidade da interface deve estar entre 35 e 100."),
    )
    for key, low, high, message in numeric_ranges:
        value = result[key]
        if type(value) is not int or not low <= value <= high:
            raise ValueError(tr(message))

    if not isinstance(result["scaling"], str) or result["scaling"] not in _SCALING:
        raise ValueError(tr("Escala inválida."))
    result["language"] = normalize_language(result["language"])


def _normalize_playlists(raw_playlists: Any) -> dict[str, list[str]]:
    if not isinstance(raw_playlists, dict):
        raise ValueError(tr("Playlists deve ser um objeto de nomes e listas de IDs."))
    playlists: dict[str, list[str]] = {}
    seen_names: set[str] = set()
    for raw_name, raw_ids in raw_playlists.items():
        name = normalize_playlist_name(raw_name)
        folded_name = name.casefold()
        if folded_name in seen_names:
            raise ValueError(tr("Nome de playlist duplicado: {name}.", name=name))
        seen_names.add(folded_name)
        if not isinstance(raw_ids, list):
            raise ValueError(tr("A playlist {name} deve conter uma lista de IDs.", name=name))
        playlists[name] = list(dict.fromkeys(normalize_id(value) for value in raw_ids))
    return playlists


def _normalize_active_playlist(active: Any, playlists: dict[str, list[str]]) -> str | None:
    if active is None:
        return None
    normalized = normalize_playlist_name(active)
    matching = [name for name in playlists if name.casefold() == normalized.casefold()]
    if not matching:
        raise ValueError(tr("Playlist ativa não encontrada: {name}.", name=normalized))
    return matching[0]


def _normalize_wallpaper_properties(raw_value: Any) -> dict[str, dict[str, bool]]:
    if not isinstance(raw_value, dict):
        raise ValueError(tr("As opções dos wallpapers devem ser um objeto."))

    wallpaper_properties: dict[str, dict[str, bool]] = {}
    for raw_id, raw_properties in raw_value.items():
        wallpaper_id = normalize_id(raw_id)
        if not isinstance(raw_properties, dict) or len(raw_properties) > 256:
            raise ValueError(tr("As opções de cada wallpaper devem ser um objeto válido."))

        properties: dict[str, bool] = {}
        for name, value in raw_properties.items():
            if not isinstance(name, str) or not _PROPERTY_PATTERN.fullmatch(name):
                raise ValueError(tr("Nome de opção de wallpaper inválido."))
            if type(value) is not bool:
                raise ValueError(tr("As opções de wallpaper devem ser verdadeiras ou falsas."))
            properties[name] = value
        if properties:
            wallpaper_properties[wallpaper_id] = properties
    return wallpaper_properties


def _validate_renderer_path(renderer: Any) -> str:
    if not isinstance(renderer, str) or not renderer or len(renderer) > 4096:
        raise ValueError(tr("Caminho do renderizador inválido."))
    if renderer != "auto" and (not Path(renderer).is_absolute() or "\x00" in renderer):
        raise ValueError(tr("O caminho do renderizador deve ser absoluto ou 'auto'."))
    return renderer


def validate_config(config: dict[str, Any]) -> dict[str, Any]:
    if not isinstance(config, dict):
        raise ValueError(tr("A configuração deve ser um objeto JSON."))
    unknown = set(config) - set(DEFAULT_CONFIG)
    if unknown:
        raise ValueError(tr("Opções desconhecidas: {options}.", options=", ".join(sorted(unknown))))

    result = dict(DEFAULT_CONFIG)
    result.update(config)
    _validate_scalar_config(result)

    favorites = result["favorites"]
    if not isinstance(favorites, list):
        raise ValueError(tr("Favoritos deve ser uma lista de IDs."))
    result["favorites"] = list(dict.fromkeys(normalize_id(value) for value in favorites))

    playlists = _normalize_playlists(result["playlists"])
    result["playlists"] = playlists
    result["active_playlist"] = _normalize_active_playlist(result["active_playlist"], playlists)

    selected = result["selected_id"]
    result["selected_id"] = None if selected is None else normalize_id(selected)

    assignments = result["screen_assignments"]
    if not isinstance(assignments, dict):
        raise ValueError(tr("Atribuições de monitor devem ser um objeto."))
    result["screen_assignments"] = {
        validate_screen(screen): normalize_id(wallpaper_id)
        for screen, wallpaper_id in assignments.items()
    }

    result["wallpaper_properties"] = _normalize_wallpaper_properties(result["wallpaper_properties"])
    result["renderer_path"] = _validate_renderer_path(result["renderer_path"])
    return result

def _write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    data = json.dumps(value, ensure_ascii=False, indent=2).encode("utf-8") + b"\n"
    fd, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        os.fchmod(fd, 0o600)
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def load_config() -> dict[str, Any]:
    try:
        data = json.loads(CONFIG_FILE.read_text(encoding="utf-8"))
        # Earlier desktop app previews exposed a pause switch. Ignore it so
        # existing settings survive the continuous-rendering migration.
        if isinstance(data, dict):
            data.pop("pause_on_cover", None)
        return validate_config(data)
    except (OSError, ValueError, TypeError):
        return validate_config({})


def save_config(config: dict[str, Any]) -> dict[str, Any]:
    validated = validate_config(config)
    _write_json(CONFIG_FILE, validated)
    return validated


def load_ui_preferences(
    legacy_config: dict[str, Any] | None = None,
) -> dict[str, int | bool]:
    """Load frontend-only appearance and session preferences."""
    try:
        values = json.loads(UI_PREFERENCES_FILE.read_text(encoding="utf-8"))
    except FileNotFoundError:
        values = legacy_config
    except (OSError, ValueError):
        values = legacy_config
    if not isinstance(values, dict):
        values = {}

    result: dict[str, int | bool] = {}
    for key, default, low, high in (
        ("ui_hue", 24, 0, 360),
        ("ui_intensity", 88, 35, 100),
    ):
        value = values.get(key, default)
        result[key] = value if type(value) is int and low <= value <= high else default
    for key in ("minimize_to_tray", "start_app_with_session"):
        value = values.get(key, False)
        result[key] = value if type(value) is bool else False
    return result


def save_ui_preferences(preferences: dict[str, Any]) -> dict[str, int | bool]:
    """Persist GUI-only preferences without sending them to the daemon."""
    if not isinstance(preferences, dict):
        raise ValueError(tr("As preferências da interface devem ser um objeto."))
    result = load_ui_preferences(load_config())
    for key, low, high in (("ui_hue", 0, 360), ("ui_intensity", 35, 100)):
        value = preferences.get(key, result[key])
        if type(value) is not int or not low <= value <= high:
            raise ValueError(tr("Preferência de aparência inválida: {key}.", key=key))
        result[key] = value
    for key in ("minimize_to_tray", "start_app_with_session"):
        value = preferences.get(key, result[key])
        if type(value) is not bool:
            raise ValueError(tr("Preferência da interface inválida: {key}.", key=key))
        result[key] = value
    _write_json(UI_PREFERENCES_FILE, result)
    return result


def set_app_autostart(enabled: bool) -> None:
    """Create or remove this user's graphical-session autostart entry."""
    if type(enabled) is not bool:
        raise ValueError(tr("A opção de inicialização deve ser verdadeira ou falsa."))
    autostart_file = CONFIG_DIR.parent / "autostart" / "linux-wallpaperengine-app.desktop"
    if not enabled:
        try:
            autostart_file.unlink()
        except FileNotFoundError:
            pass
        return

    launcher: Path | None = None
    candidate = Path.home() / ".local/bin/linux-wallpaperengine-app"
    if candidate.is_file() and os.access(candidate, os.X_OK):
        launcher = candidate
    else:
        located = shutil.which("linux-wallpaperengine-app")
        if located:
            launcher = Path(located).resolve()
    if launcher is None or not launcher.is_file() or not os.access(launcher, os.X_OK):
        raise RuntimeError(tr("Não foi possível localizar o inicializador do aplicativo."))
    if any(ord(character) < 32 for character in str(launcher)):
        raise ValueError(tr("O caminho do inicializador contém caracteres inválidos."))

    desktop_exec = str(launcher).replace("\\", "\\\\").replace('"', '\\"').replace("%", "%%")
    desktop_entry = (
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Linux Wallpaper Engine\n"
        "Comment=Open Linux Wallpaper Engine after login\n"
        f'Exec="{desktop_exec}"\n'
        "Icon=linux-wallpaperengine-app\n"
        "Terminal=false\n"
        "X-GNOME-Autostart-enabled=true\n"
    )
    autostart_file.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    fd, temporary = tempfile.mkstemp(prefix=f".{autostart_file.name}.", dir=autostart_file.parent)
    try:
        os.fchmod(fd, 0o600)
        with os.fdopen(fd, "w", encoding="utf-8") as stream:
            stream.write(desktop_entry)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, autostart_file)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def save_status(status: dict[str, Any]) -> None:
    _write_json(STATUS_FILE, status)


def read_status() -> dict[str, Any]:
    try:
        status = json.loads(STATUS_FILE.read_text(encoding="utf-8"))
        if not isinstance(status, dict):
            raise ValueError(tr("Estado inválido"))
    except (OSError, ValueError):
        status = {}
    result: dict[str, Any] = {
        "running": False,
        "renderer_running": False,
        "current_id": None,
        "screens": [],
        "screen_wallpapers": {},
        "next_change_at": None,
        "error": None,
        "config": load_config(),
    }
    result.update(status)
    for obsolete in ("paused", "pause_available", "pause_error"):
        result.pop(obsolete, None)
    # A saved status can outlive an unclean daemon exit.
    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as probe:
            probe.settimeout(0.15)
            probe.connect(str(SOCKET_FILE))
    except OSError:
        result.update(running=False, renderer_running=False, next_change_at=None)
    return result


def steamapps_roots() -> list[Path]:
    home = Path.home()
    data_home = _xdg_dir("XDG_DATA_HOME", home / ".local/share")
    defaults = [
        data_home / "Steam/steamapps",
        home / ".steam/steam/steamapps",
        home / ".var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps",
        home / "snap/steam/common/.local/share/Steam/steamapps",
    ]
    candidates = list(defaults)
    for steamapps in defaults:
        for listing in (
            steamapps / "libraryfolders.vdf",
            steamapps.parent / "config/libraryfolders.vdf",
        ):
            for library in _steam_library_paths(listing):
                candidates.append(
                    library if library.name == "steamapps" else library / "steamapps"
                )
    roots: list[Path] = []
    seen: set[Path] = set()
    for candidate in candidates:
        try:
            canonical = candidate.resolve()
        except (OSError, RuntimeError, ValueError):
            continue
        if canonical not in seen:
            roots.append(canonical)
            seen.add(canonical)
    return roots


def _consume_quoted_vdf_token(source: str, position: int) -> tuple[str, int]:
    position += 1
    value: list[str] = []
    while position < len(source) and source[position] != '"':
        if (
            source[position] == "\\"
            and position + 1 < len(source)
            and source[position + 1] in ('"', "\\")
        ):
            position += 1
        value.append(source[position])
        position += 1
    return "".join(value), position + 1


def _consume_bare_vdf_token(source: str, position: int) -> tuple[str, int]:
    end = position
    while end < len(source) and not source[end].isspace() and source[end] not in '{}"':
        end += 1
    return source[position:end], end


def _tokenize_vdf(source: str) -> list[str]:
    tokens: list[str] = []
    position = 0
    while position < len(source):
        char = source[position]
        if char.isspace():
            position += 1
            continue
        if source.startswith("//", position):
            end = source.find("\n", position)
            position = len(source) if end < 0 else end + 1
            continue
        if char in "{}":
            tokens.append(char)
            position += 1
            continue
        if char == '"':
            token, position = _consume_quoted_vdf_token(source, position)
        else:
            token, position = _consume_bare_vdf_token(source, position)
        tokens.append(token)
    return tokens

def _read_vdf_object(tokens: list[str], index: int, depth: int = 0) -> tuple[dict[str, Any], int]:
    if depth > 16:
        return {}, len(tokens)
    values: dict[str, Any] = {}
    while index < len(tokens):
        key = tokens[index]
        index += 1
        if key == "}" or index >= len(tokens):
            break
        value = tokens[index]
        index += 1
        if value == "{":
            values[key], index = _read_vdf_object(tokens, index, depth + 1)
        elif value == "}":
            break
        else:
            values[key] = value
    return values, index


def _steam_library_paths(listing: Path) -> list[Path]:
    """Read both current and older Steam libraryfolders.vdf layouts."""
    try:
        source = listing.read_text(encoding="utf-8-sig")
    except (OSError, UnicodeError):
        return []

    tokens = _tokenize_vdf(source)
    if len(tokens) < 2 or tokens[0].casefold() != "libraryfolders" or tokens[1] != "{":
        return []

    entries, _ = _read_vdf_object(tokens, 2)
    libraries: list[Path] = []
    for key, value in entries.items():
        if not key.isdigit():
            continue
        raw = value.get("path") if isinstance(value, dict) else value
        if isinstance(raw, str) and raw:
            libraries.append(Path(raw).expanduser())
    return libraries

def _libraryfolders_paths(steamapps: Path) -> list[Path]:
    """Return Steam library roots for the existing catalog callers."""
    return [
        library if library.name == "steamapps" else library / "steamapps"
        for library in _steam_library_paths(steamapps / "libraryfolders.vdf")
        if library.is_absolute()
    ]


def _preview_for(project_dir: Path, raw: Any) -> str | None:
    candidates = [raw] if isinstance(raw, str) else []
    candidates.extend(("preview.jpg", "preview.jpeg", "preview.png", "preview.gif"))
    directory = project_dir.resolve()
    for candidate in candidates:
        if not candidate:
            continue
        path = (directory / candidate).resolve()
        if path.is_relative_to(directory) and path.is_file():
            return str(path)
    return None


def _animation_for(project_dir: Path, raw: Any) -> str | None:
    """Return the Workshop GIF preview, even when project.json names a still."""
    candidates = [raw] if isinstance(raw, str) and Path(raw).suffix.casefold() == ".gif" else []
    candidates.append("preview.gif")
    directory = project_dir.resolve()
    for candidate in candidates:
        if not candidate:
            continue
        path = (directory / candidate).resolve()
        if path.is_relative_to(directory) and path.is_file() and path.suffix.casefold() == ".gif":
            return str(path)
    return None


def _tags_for(raw: Any) -> list[str]:
    if isinstance(raw, list):
        return [item for item in raw if isinstance(item, str) and item]
    if isinstance(raw, str):
        return [part.strip() for part in raw.split(",") if part.strip()]
    return []


def scan_catalog() -> dict[str, dict[str, Any]]:
    """Find installed scene/video Workshop projects without modifying Steam data."""
    result: dict[str, dict[str, Any]] = {}
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
        if not workshop.is_dir():
            continue
        local_assets = steamapps / "common/wallpaper_engine/assets"
        assets = local_assets if local_assets.is_dir() else (fallback_assets or local_assets)
        try:
            projects = sorted(workshop.glob("*/project.json"))
        except OSError:
            continue
        for project in projects:
            wallpaper_id = project.parent.name
            if not _ID_PATTERN.fullmatch(wallpaper_id) or wallpaper_id in result:
                continue
            try:
                data = json.loads(project.read_text(encoding="utf-8-sig"))
            except (OSError, ValueError):
                continue
            if not isinstance(data, dict):
                continue
            kind = str(data.get("type", "")).lower()
            if kind not in {"scene", "video"}:
                continue
            raw_title = data.get("title")
            title = raw_title.strip() if isinstance(raw_title, str) and raw_title.strip() else wallpaper_id
            result[wallpaper_id] = {
                "title": title,
                "type": kind,
                "path": str(project.parent),
                "assets": str(assets),
                "preview": _preview_for(project.parent, data.get("preview")),
                "preview_animation": _animation_for(project.parent, data.get("preview")),
                "tags": _tags_for(data.get("tags")),
                "properties": _boolean_properties(data),
            }
    return result


def _boolean_properties(project: dict[str, Any]) -> dict[str, dict[str, Any]]:
    """Expose the author's boolean Workshop options for the details panel."""
    general = project.get("general")
    raw_properties = general.get("properties") if isinstance(general, dict) else None
    if not isinstance(raw_properties, dict):
        return {}
    result: dict[str, dict[str, Any]] = {}
    for name, value in raw_properties.items():
        if (
            not isinstance(name, str)
            or not _PROPERTY_PATTERN.fullmatch(name)
            or not isinstance(value, dict)
            or str(value.get("type", "")).lower() not in {"bool", "boolean"}
            or type(value.get("value")) is not bool
        ):
            continue
        label = value.get("text")
        result[name] = {
            "label": label.strip() if isinstance(label, str) and label.strip() else name,
            "value": value["value"],
        }
    return result


def _kscreen_outputs() -> list[str]:
    if not shutil.which("kscreen-doctor"):
        return []
    try:
        result = subprocess.run(
            ["kscreen-doctor", "-j"], capture_output=True, text=True,
            timeout=5, check=True,
        )
        data = json.loads(result.stdout)
    except (OSError, subprocess.SubprocessError, ValueError, TypeError):
        return []
    if not isinstance(data, dict):
        return []
    names = [
        output["name"] for output in data.get("outputs", [])
        if isinstance(output, dict) and output.get("enabled")
        and output.get("connected") and isinstance(output.get("name"), str)
    ]
    return list(dict.fromkeys(names))


def _native_wayland_session() -> bool:
    session_type = os.environ.get("XDG_SESSION_TYPE")
    return session_type == "wayland" or (
        session_type != "x11"
        and (not os.environ.get("DISPLAY") or bool(os.environ.get("WAYLAND_DISPLAY")))
    )


def _kde_session() -> bool:
    desktop = f"{os.environ.get('XDG_CURRENT_DESKTOP', '')}:{os.environ.get('XDG_SESSION_DESKTOP', '')}".lower()
    return "kde" in desktop or "plasma" in desktop


def _xrandr_outputs() -> list[str]:
    if not shutil.which("xrandr"):
        return []
    try:
        result = subprocess.run(
            ["xrandr", "--query"], capture_output=True, text=True,
            timeout=5, check=True,
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


def detect_outputs() -> list[str]:
    """Discover active outputs using an available desktop/session backend."""
    if _kde_session() or _native_wayland_session():
        names = _kscreen_outputs()
        if names or _native_wayland_session():
            return names
        return _xrandr_outputs()
    names = _xrandr_outputs()
    if names:
        return names
    return _kscreen_outputs()
