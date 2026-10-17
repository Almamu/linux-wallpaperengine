#!/usr/bin/env bash
set -euo pipefail

# Install only the Python desktop app. The renderer is installed separately.
[[ -n "${HOME:-}" && "${HOME}" != / && "${EUID}" -ne 0 ]] || {
    printf 'Error: run this installer as your normal desktop user, without sudo.\n' >&2
    exit 1
}
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_PACKAGE="${ROOT}/app/wallpaper_engine_app"
APP_LIB="${HOME}/.local/lib/linux-wallpaperengine-app"
PACKAGE_DIR="${APP_LIB}/wallpaper_engine_app"
APP_BIN="${HOME}/.local/bin/linux-wallpaperengine-app"
DESKTOP_FILE="${HOME}/.local/share/applications/linux-wallpaperengine-app.desktop"
ICON_FILE="${HOME}/.local/share/icons/hicolor/scalable/apps/linux-wallpaperengine-app.svg"
SERVICE_FILE="${HOME}/.config/systemd/user/linux-wallpaperengine-app.service"
AUTOSTART_FILE="${HOME}/.config/autostart/linux-wallpaperengine-app.desktop"
SERVICE="linux-wallpaperengine-app.service"
LEGACY_SERVICE="linux-wallpaperengine-rotation.service"

die() {
    printf 'Error: %s\n' "$*" >&2
    exit 1
}

usage() {
    cat <<'USAGE'
Usage: ./app/install.sh [--uninstall]

Installs the GTK desktop app for the current user (no sudo): the Python
package in ~/.local/lib/linux-wallpaperengine-app, a launcher in
~/.local/bin/linux-wallpaperengine-app, a desktop menu entry with icon, and
the linux-wallpaperengine-app.service systemd user service.

  --uninstall   Removes the files and service listed above.
                User settings (~/.config/linux-wallpaperengine) are kept.
  -h, --help    Show this help.

Requirements: python3 with PyGObject plus GTK 4 and GdkPixbuf introspection,
systemctl with a --user manager, and the linux-wallpaperengine renderer in
PATH or ~/.local/bin. Optional: the GTK 3 Ayatana AppIndicator/AppIndicator3
introspection, needed only for minimize-to-tray.
USAGE
}

case "${1:-}" in
    '') ;;
    --uninstall) UNINSTALL=1 ;;
    -h|--help) usage; exit 0 ;;
    *) usage >&2; exit 2 ;;
esac
[[ $# -le 1 ]] || { usage >&2; exit 2; }

command -v systemctl >/dev/null 2>&1 || \
    die 'systemctl is required to install and manage the systemd user service.'
systemctl --user show-environment >/dev/null 2>&1 || \
    die 'No systemd --user manager is available. Run this installer from your logged-in desktop session, not over SSH or a text console.'

if [[ "${UNINSTALL:-0}" -eq 1 ]]; then
    if systemctl --user cat "${SERVICE}" >/dev/null 2>&1; then
        systemctl --user disable --now "${SERVICE}" || \
            die "Could not stop ${SERVICE}; installed files were kept."
    fi
    rm -rf -- "${PACKAGE_DIR}"
    rm -f -- "${APP_BIN}" "${DESKTOP_FILE}" "${ICON_FILE}" "${SERVICE_FILE}"
    # The "start with the session" switch writes this entry from the GUI; it
    # would point at a launcher this uninstall just removed.
    rm -f -- "${AUTOSTART_FILE}"
    systemctl --user daemon-reload
    printf 'Desktop app uninstalled: removed the app package, the launcher, the menu entry, the icon, the autostart entry and %s.\n' "${SERVICE}"
    printf 'Your preferences (~/.config/linux-wallpaperengine), the renderer and the old rotation service (kept as-is) were not touched.\n'
    exit 0
fi

[[ -f "${SOURCE_PACKAGE}/__main__.py" ]] || die 'App package not found. Run this installer from the repository checkout (it must contain app/wallpaper_engine_app).'
[[ -f "${ROOT}/app/linux-wallpaperengine-app.svg" ]] || die 'App icon not found (expected app/linux-wallpaperengine-app.svg in the repository checkout).'

PYTHON_BIN="$(command -v python3 || true)"
[[ -n "${PYTHON_BIN}" ]] || \
    die 'python3 is required (Arch: sudo pacman -S python; Debian/Ubuntu: sudo apt install python3; Fedora: sudo dnf install python3).'
"${PYTHON_BIN}" -c 'import gi; gi.require_version("Gtk", "4.0"); gi.require_version("GdkPixbuf", "2.0"); from gi.repository import Gtk, GdkPixbuf' 2>/dev/null || \
    die 'PyGObject with GTK 4 and GdkPixbuf introspection is required (Arch: sudo pacman -S python-gobject gtk4 gdk-pixbuf2; Debian/Ubuntu: sudo apt install python3-gi gir1.2-gtk-4.0 gir1.2-gdkpixbuf-2.0; Fedora: sudo dnf install python3-gobject gtk4 gdk-pixbuf2).'
if ! "${PYTHON_BIN}" -c '
import importlib

import gi

gi.require_version("Gtk", "3.0")
for namespace in ("AyatanaAppIndicator3", "AppIndicator3"):
    try:
        gi.require_version(namespace, "0.1")
        break
    except ValueError:
        continue
else:
    raise SystemExit(1)
importlib.import_module("gi.repository." + namespace)
from gi.repository import Gtk
' 2>/dev/null; then
    # Non-fatal: only minimize-to-tray depends on this. Point at the exact
    # package so the user can fix it without searching.
    TRAY_INSTALL_CMD=''
    if command -v pacman >/dev/null 2>&1; then
        TRAY_INSTALL_CMD='sudo pacman -S libayatana-appindicator'
    elif command -v apt-get >/dev/null 2>&1 || command -v apt >/dev/null 2>&1; then
        TRAY_INSTALL_CMD='sudo apt install gir1.2-ayatanaappindicator3-0.1'
    elif command -v dnf >/dev/null 2>&1; then
        TRAY_INSTALL_CMD='sudo dnf install libayatana-appindicator-gtk3'
    elif command -v zypper >/dev/null 2>&1; then
        TRAY_INSTALL_CMD='sudo zypper install typelib-1_0-AyatanaAppIndicator3-0_1'
    fi
    printf 'Warning: only minimize-to-tray needs the tray dependency (GTK 3 plus the Ayatana AppIndicator or AppIndicator3 introspection); the rest of the app works without it.\n' >&2
    if [[ -n "${TRAY_INSTALL_CMD}" ]]; then
        printf 'To enable it, run: %s\n' "${TRAY_INSTALL_CMD}" >&2
    else
        printf 'To enable it, install your distribution'\''s Ayatana AppIndicator GTK 3 introspection package.\n' >&2
    fi
fi

# Display discovery is session-specific. XRandR cannot discover native Wayland
# outputs, so it must never satisfy the KDE Wayland requirement by itself.
case "${XDG_CURRENT_DESKTOP:-}:${XDG_SESSION_TYPE:-}" in
    *KDE*:wayland|*kde*:wayland|*Plasma*:wayland|*plasma*:wayland)
        command -v kscreen-doctor >/dev/null 2>&1 || \
            die 'kscreen-doctor is required for display discovery in a KDE Plasma Wayland session.'
        ;;
    *GNOME*:wayland|*gnome*:wayland)
        printf 'Warning: GNOME/Mutter Wayland does not provide the wlr-layer-shell protocol required by the renderer; the control app can be installed, but desktop wallpaper rendering is not supported in this session.\n' >&2
        ;;
    *:wayland)
        if ! command -v kscreen-doctor >/dev/null 2>&1; then
            printf 'Warning: no supported Wayland display-discovery backend was found; monitor discovery may fail.\n' >&2
        fi
        ;;
    *)
        if ! command -v kscreen-doctor >/dev/null 2>&1 && ! command -v xrandr >/dev/null 2>&1; then
            printf 'Warning: neither kscreen-doctor nor xrandr is available; monitor discovery may fail.\n' >&2
        fi
        ;;
esac
if ! command -v linux-wallpaperengine >/dev/null 2>&1 && \
   [[ ! -x "${HOME}/.local/bin/linux-wallpaperengine" ]]; then
    die 'The linux-wallpaperengine renderer was not found in PATH or ~/.local/bin. Install the renderer first (see "Build from source" in the README), then run this installer again.'
fi

install -d "${APP_LIB}" "$(dirname -- "${APP_BIN}")" \
    "$(dirname -- "${DESKTOP_FILE}")" "$(dirname -- "${ICON_FILE}")" \
    "$(dirname -- "${SERVICE_FILE}")"
STAGING="$(mktemp -d "${APP_LIB}/.install.XXXXXXXX")"
PACKAGE_REPLACED=0
LEGACY_ENABLED=0
LEGACY_ACTIVE=0
NEW_ENABLED=0
NEW_ACTIVE=0
NEW_INSTALLED=0

systemctl --user is-enabled --quiet "${LEGACY_SERVICE}" && LEGACY_ENABLED=1 || true
systemctl --user is-active --quiet "${LEGACY_SERVICE}" && LEGACY_ACTIVE=1 || true
systemctl --user is-enabled --quiet "${SERVICE}" && NEW_ENABLED=1 || true
systemctl --user is-active --quiet "${SERVICE}" && NEW_ACTIVE=1 || true
[[ -f "${SERVICE_FILE}" ]] && NEW_INSTALLED=1 || true

# A fresh install starts the daemon. An update respects the app's autostart
# switch and a user who deliberately stopped the service.
TARGET_ENABLED="${NEW_ENABLED}"
TARGET_ACTIVE="${NEW_ACTIVE}"
if [[ "${NEW_INSTALLED}" -eq 0 ]]; then
    TARGET_ENABLED=1
    TARGET_ACTIVE=1
fi

restore_file() {
    local name="$1" destination="$2"
    if [[ -e "${STAGING}/backup/${name}" ]]; then
        install -Dm644 "${STAGING}/backup/${name}" "${destination}"
    else
        rm -f -- "${destination}"
    fi
}

finish() {
    local code="$1"
    trap - EXIT
    if [[ "${code}" -ne 0 && "${PACKAGE_REPLACED}" -eq 1 ]]; then
        printf 'Install failed; restoring the previous app and service state.\n' >&2
        systemctl --user disable --now "${SERVICE}" >/dev/null 2>&1 || true
        rm -rf -- "${PACKAGE_DIR}"
        if [[ -d "${STAGING}/old_package" ]]; then
            mv -- "${STAGING}/old_package" "${PACKAGE_DIR}" || true
        fi
        if [[ -e "${STAGING}/backup/app_bin" ]]; then
            install -Dm755 "${STAGING}/backup/app_bin" "${APP_BIN}" || true
        else
            rm -f -- "${APP_BIN}"
        fi
        restore_file desktop "${DESKTOP_FILE}" || true
        restore_file icon "${ICON_FILE}" || true
        restore_file service "${SERVICE_FILE}" || true
        systemctl --user daemon-reload >/dev/null 2>&1 || true
        if [[ "${NEW_ENABLED}" -eq 1 ]]; then
            systemctl --user enable "${SERVICE}" >/dev/null 2>&1 || true
        fi
        if [[ "${NEW_ACTIVE}" -eq 1 ]]; then
            systemctl --user start "${SERVICE}" >/dev/null 2>&1 || true
        fi
        if [[ "${LEGACY_ENABLED}" -eq 1 ]]; then
            systemctl --user enable "${LEGACY_SERVICE}" >/dev/null 2>&1 || true
        fi
        if [[ "${LEGACY_ACTIVE}" -eq 1 ]]; then
            systemctl --user start "${LEGACY_SERVICE}" >/dev/null 2>&1 || true
        fi
    fi
    rm -rf -- "${STAGING}"
    exit "${code}"
}
trap 'finish $?' EXIT

install -d "${STAGING}/new_package" "${STAGING}/backup"
cp -a -- "${SOURCE_PACKAGE}/." "${STAGING}/new_package/"
find "${STAGING}/new_package" -type d -name __pycache__ -prune -exec rm -rf -- {} +
find "${STAGING}/new_package" -type f \( -name '*.pyc' -o -name '*.pyo' \) -delete
if [[ -e "${APP_BIN}" ]]; then cp -a -- "${APP_BIN}" "${STAGING}/backup/app_bin"; fi
if [[ -e "${DESKTOP_FILE}" ]]; then cp -a -- "${DESKTOP_FILE}" "${STAGING}/backup/desktop"; fi
if [[ -e "${ICON_FILE}" ]]; then cp -a -- "${ICON_FILE}" "${STAGING}/backup/icon"; fi
if [[ -e "${SERVICE_FILE}" ]]; then cp -a -- "${SERVICE_FILE}" "${STAGING}/backup/service"; fi

printf -v PYTHON_QUOTED '%q' "${PYTHON_BIN}"
cat > "${STAGING}/app_bin" <<EOF
#!/usr/bin/env bash
set -euo pipefail
export PYTHONPATH="\${HOME}/.local/lib/linux-wallpaperengine-app\${PYTHONPATH:+:\${PYTHONPATH}}"
export PATH="\${HOME}/.local/bin:\${PATH}"
export LINUX_WALLPAPERENGINE_ICON_FILE="\${HOME}/.local/share/icons/hicolor/scalable/apps/linux-wallpaperengine-app.svg"
export PYTHONDONTWRITEBYTECODE=1
exec ${PYTHON_QUOTED} -m wallpaper_engine_app "\$@"
EOF
chmod 755 "${STAGING}/app_bin"

# A desktop launcher cannot assume ~/.local/bin is in the graphical PATH.
DESKTOP_EXEC="${APP_BIN//\\/\\\\}"
DESKTOP_EXEC="${DESKTOP_EXEC//\"/\\\"}"
DESKTOP_EXEC="${DESKTOP_EXEC//\$/\\\$}"
DESKTOP_EXEC="${DESKTOP_EXEC//\`/\\\`}"
while IFS= read -r line || [[ -n "${line}" ]]; do
    if [[ "${line}" == 'Exec=@APP_EXEC@' ]]; then
        printf 'Exec="%s"\n' "${DESKTOP_EXEC}"
    else
        printf '%s\n' "${line}"
    fi
done < "${ROOT}/app/linux-wallpaperengine-app.desktop" > "${STAGING}/desktop"

if [[ -d "${PACKAGE_DIR}" ]]; then
    mv -- "${PACKAGE_DIR}" "${STAGING}/old_package"
fi
PACKAGE_REPLACED=1
mv -- "${STAGING}/new_package" "${PACKAGE_DIR}"
install -Dm755 "${STAGING}/app_bin" "${APP_BIN}"
install -Dm644 "${STAGING}/desktop" "${DESKTOP_FILE}"
install -Dm644 "${ROOT}/app/linux-wallpaperengine-app.svg" "${ICON_FILE}"
install -Dm644 "${ROOT}/app/linux-wallpaperengine-app.service" "${SERVICE_FILE}"
systemctl --user daemon-reload

# Only migrate the old service when the new one will be enabled or running.
# An update of a disabled and stopped app leaves a running legacy service alone.
if [[ ( "${TARGET_ENABLED}" -eq 1 || "${TARGET_ACTIVE}" -eq 1 ) &&
      ( "${LEGACY_ENABLED}" -eq 1 || "${LEGACY_ACTIVE}" -eq 1 ) ]]; then
    printf 'Stopping the legacy rotation service; its files and settings are kept.\n'
    systemctl --user disable --now "${LEGACY_SERVICE}"
fi

if [[ "${NEW_INSTALLED}" -eq 0 ]]; then
    systemctl --user enable "${SERVICE}"
fi
if [[ "${TARGET_ACTIVE}" -eq 1 ]]; then
    systemctl --user restart "${SERVICE}"
    sleep 1
    systemctl --user is-active --quiet "${SERVICE}" || \
        die "${SERVICE} did not stay active; check: journalctl --user -u ${SERVICE} -e"
else
    # This also handles a service that became active while the files copied.
    systemctl --user stop "${SERVICE}"
fi

printf 'Desktop app installed for this user:\n'
printf '  app package: %s\n' "${PACKAGE_DIR}"
printf '  launcher:    %s\n' "${APP_BIN}"
printf '  menu entry:  %s\n' "${DESKTOP_FILE}"
printf '  service:     %s\n' "${SERVICE_FILE}"
printf 'Launch "Linux Wallpaper Engine" from your app menu, or run the launcher above.\n'
if [[ "${TARGET_ACTIVE}" -eq 1 ]]; then
    printf 'Service active: %s (check it with: systemctl --user status %s).\n' "${SERVICE}" "${SERVICE}"
else
    printf 'Service stopped: %s (start it with: systemctl --user start %s).\n' "${SERVICE}" "${SERVICE}"
fi
printf 'To remove the app later, keeping your settings: %s --uninstall\n' "${ROOT}/app/install.sh"
