#!/usr/bin/env bash
set -euo pipefail

# One-shot installer for this fork: system packages (one sudo step), the
# renderer built from this checkout when it is missing, and the GTK desktop
# app through app/install.sh.

[[ -n "${HOME:-}" && "${HOME}" != / && "${EUID}" -ne 0 ]] || {
    printf 'Error: run this installer as your normal desktop user; only the package step uses sudo.\n' >&2
    exit 1
}
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT}/build"
RENDERER_PREFIX="${HOME}/.local/opt/linux-wallpaperengine"
RENDERER_BIN="${RENDERER_PREFIX}/linux-wallpaperengine"
RENDERER_LINK="${HOME}/.local/bin/linux-wallpaperengine"
# Written only when THIS script builds the renderer; --uninstall uses it to
# decide whether the prefix and symlink belong to us and may be removed.
RENDERER_MARKER="${RENDERER_PREFIX}/.lwe-installed-by-install-sh"

die() {
    printf 'Error: %s\n' "$*" >&2
    exit 1
}

usage() {
    cat <<'USAGE'
Usage: ./install.sh [--dry-run] [--uninstall]

Installs everything for the current user in one run:

  1. The system packages required by the GTK app and the renderer build.
     This is the only step that needs sudo, and it runs only after you
     confirm the printed package command.
  2. The linux-wallpaperengine renderer, built from this checkout and
     installed under ~/.local/opt/linux-wallpaperengine with a symlink in
     ~/.local/bin (skipped when a renderer is already in PATH or
     ~/.local/bin). The configure step downloads the Chromium Embedded
     Framework distribution and needs network access.
  3. The desktop app via app/install.sh, including its
     linux-wallpaperengine-app.service systemd user service.

  --dry-run    Print every step and the package commands without changing
               anything and without asking for sudo.
  --uninstall  Removes the desktop app and, when this script built it, the
               renderer as well. User settings (~/.config/linux-wallpaperengine)
               and your build directory are kept.
  -h, --help   Show this help.
USAGE
}

UNINSTALL=0
DRY_RUN=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --uninstall) UNINSTALL=1 ;;
        --dry-run) DRY_RUN=1 ;;
        -h|--help) usage; exit 0 ;;
        *) usage >&2; exit 2 ;;
    esac
    shift
done

# --- Uninstall -------------------------------------------------------------

if [[ "${UNINSTALL}" -eq 1 ]]; then
    if [[ "${DRY_RUN}" -eq 1 ]]; then
        printf 'Dry run: nothing will be removed.\n'
        printf 'Would run: %s --uninstall\n' "${ROOT}/app/install.sh"
        if [[ -f "${RENDERER_MARKER}" ]]; then
            if [[ -L "${RENDERER_LINK}" ]]; then
                case "$(readlink -- "${RENDERER_LINK}")" in
                    "${RENDERER_PREFIX}"/*)
                        printf 'Would remove the symlink %s (it points into %s).\n' \
                            "${RENDERER_LINK}" "${RENDERER_PREFIX}" ;;
                    *)
                        printf 'Would keep %s (it does not point into %s).\n' \
                            "${RENDERER_LINK}" "${RENDERER_PREFIX}" ;;
                esac
            fi
            printf 'Would remove %s (the marker file says this script built it).\n' \
                "${RENDERER_PREFIX}"
        else
            printf 'The renderer was not built by this script; it would be left in place.\n'
        fi
        exit 0
    fi
    "${ROOT}/app/install.sh" --uninstall
    if [[ -f "${RENDERER_MARKER}" ]]; then
        if [[ -L "${RENDERER_LINK}" ]]; then
            case "$(readlink -- "${RENDERER_LINK}")" in
                "${RENDERER_PREFIX}"/*)
                    rm -f -- "${RENDERER_LINK}"
                    printf 'Removed the renderer symlink %s.\n' "${RENDERER_LINK}" ;;
                *)
                    printf 'Kept %s (it does not point into %s).\n' \
                        "${RENDERER_LINK}" "${RENDERER_PREFIX}" ;;
            esac
        fi
        rm -rf -- "${RENDERER_PREFIX}"
        printf 'Removed %s (the renderer was built by ./install.sh).\n' "${RENDERER_PREFIX}"
    else
        printf 'The renderer was not installed by ./install.sh (you built it or a package manager installed it); leaving it in place.\n'
        printf 'To remove it manually, delete %s and %s, or uninstall the package that provided it.\n' \
            "${RENDERER_BIN}" "${RENDERER_LINK}"
    fi
    printf 'Your preferences (~/.config/linux-wallpaperengine) and the build directory (%s) were kept.\n' \
        "${BUILD_DIR}"
    exit 0
fi

# --- Package lists ---------------------------------------------------------
# Package names per family. Evidence for each group:
#   GUI:            README.md "Install the desktop app" prerequisites block
#                   (python/python-gobject/gtk4/gdk-pixbuf2 on Arch,
#                   python3/python3-gi/gir1.2-gtk-4.0/gir1.2-gdkpixbuf-2.0 on
#                   Debian, python3/python3-gobject/gtk4/gdk-pixbuf2 on Fedora).
#   Build tool:     CMakeLists.txt:1 (cmake), CMakeLists.txt:10 (C++20), and
#                   packaging/archlinux/PKGBUILD:11 makedepends (git, cmake).
#   Renderer libs:  CMakeLists.txt:27-39 find_package calls (X11, OpenGL, GLEW,
#                   DBus, GLUT, ZLIB, SDL2, MPV, LZ4, FFMPEG, PulseAudio,
#                   Freetype), CMakeLists.txt:154/157 (wayland-cursor,
#                   wayland-protocols, wayland-egl, egl, wayland-scanner),
#                   CMakeLists.txt:217-221 (Xrandr, Xxf86vm),
#                   .github/workflows/cmake.yml:122 (exact Debian package
#                   names CI installs for all of the above),
#                   packaging/archlinux/PKGBUILD:10-11 (Arch names, plus glm
#                   and glfw; glfw is linked as a plain library at
#                   CMakeLists.txt:600 and glm headers are used, e.g.
#                   src/WallpaperEngine/Maths.h:3).
#   CEF runtime:    packaging/archlinux/PKGBUILD:10 (nss, nspr, libcups,
#                   at-spi2-core, libxcomposite, libxdamage — shared-library
#                   requirements of the downloaded CEF distribution).
# Fedora and openSUSE names for the renderer libraries are best-effort
# translations of the Debian/Arch names and are not verified by CI.

PKGS_PACMAN=(
    # GUI
    python python-gobject gtk4 gdk-pixbuf2
    # build toolchain and helpers (pkgconf provides pkg-config, used by the
    # pkg_check_modules calls in CMakeLists.txt)
    cmake gcc make git pkgconf
    # renderer libraries (lowercase sdl2 is the real target: on current
    # Arch/CachyOS it resolves to sdl2-compat, which provides "sdl2")
    glew freeglut sdl2 lz4 ffmpeg mpv libpulse freetype2 zlib
    glfw glm libx11 libxrandr libxxf86vm
    wayland wayland-protocols dbus
    # CEF runtime libraries
    nss nspr libcups at-spi2-core libxcomposite libxdamage
)
PKGS_APT=(
    # GUI
    python3 python3-gi gir1.2-gtk-4.0 gir1.2-gdkpixbuf-2.0
    # build toolchain and helpers (zlib1g-dev and libdbus-1-dev back the
    # REQUIRED finds at CMakeLists.txt:31 and :33; CI relies on the runner
    # image providing them)
    build-essential cmake git pkg-config zlib1g-dev libdbus-1-dev
    # renderer libraries (cmake.yml:122; wayland-scanner++ ships the scanner
    # used at CMakeLists.txt:157 on Ubuntu 24.04)
    libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev
    libglew-dev freeglut3-dev libsdl2-dev liblz4-dev libavcodec-dev
    libavformat-dev libavutil-dev libswscale-dev libxxf86vm-dev libglm-dev
    libglfw3-dev libmpv-dev mpv libpulse-dev libfreetype-dev
    wayland-scanner++ wayland-protocols libwayland-dev
    # CEF runtime libraries
    libnss3 libnspr4 libcups2 libxcomposite1 libxdamage1 libatspi2.0-0
)
PKGS_DNF=(
    # GUI
    python3 python3-gobject gtk4 gdk-pixbuf2
    # build toolchain and helpers
    gcc-c++ make cmake git pkgconf-pkg-config
    # renderer libraries (Fedora names, unverified by CI)
    glew-devel freeglut-devel SDL2-devel lz4-devel ffmpeg-free-devel
    libmpv-devel mpv libpulse-devel freetype-devel zlib-devel dbus-devel
    glfw-devel glm-devel
    libX11-devel libXrandr-devel libXxf86vm-devel libXinerama-devel
    libXcursor-devel libXi-devel wayland-devel wayland-protocols-devel
    # CEF runtime libraries
    nss nspr cups at-spi2-core libxcomposite libxdamage
)
PKGS_ZYPPER=(
    # GUI
    python3 python3-gobject gtk4 typelib-1_0-Gtk-4_0
    libgdk_pixbuf-2_0-0 typelib-1_0-GdkPixbuf-2_0
    # build toolchain and helpers
    gcc-c++ make cmake git pkgconf-pkg-config
    # renderer libraries (openSUSE names, unverified by CI)
    glew-devel freeglut-devel libSDL2-devel lz4-devel libavcodec-devel
    libavformat-devel libavutil-devel libswscale-devel libmpv-devel mpv
    libpulse-devel freetype2-devel zlib-devel dbus-1-devel glfw-devel
    glm-devel
    libX11-devel libXrandr-devel libXxf86vm-devel libXinerama-devel
    libXcursor-devel libXi-devel wayland-devel wayland-protocols-devel
    # CEF runtime libraries
    libnss3 libnspr4 libcups2 libXcomposite1 libXdamage1 libatspi2-0
)

# Optional extras, never blocking (README.md optional-package blocks, and the
# tray package names app/install.sh already prints).
OPT_TRAY_PACMAN=(gtk3 libayatana-appindicator)
OPT_TRAY_APT=(gir1.2-gtk-3.0 gir1.2-ayatanaappindicator3-0.1)
OPT_TRAY_DNF=(gtk3 libayatana-appindicator-gtk3)
OPT_TRAY_ZYPPER=(typelib-1_0-AyatanaAppIndicator3-0_1)
OPT_MONITOR_PACMAN=(xorg-xrandr kscreen)
OPT_MONITOR_APT=(x11-xserver-utils kscreen)
OPT_MONITOR_DNF=(xrandr kscreen)
OPT_MONITOR_ZYPPER=(xrandr kscreen)

PM=''
if command -v pacman >/dev/null 2>&1; then
    PM='pacman'
elif command -v apt-get >/dev/null 2>&1 || command -v apt >/dev/null 2>&1; then
    PM='apt'
elif command -v dnf >/dev/null 2>&1; then
    PM='dnf'
elif command -v zypper >/dev/null 2>&1; then
    PM='zypper'
fi

REQUIRED=()
OPT_TRAY=()
OPT_MONITOR=()
case "${PM}" in
    pacman) REQUIRED=("${PKGS_PACMAN[@]}"); OPT_TRAY=("${OPT_TRAY_PACMAN[@]}"); OPT_MONITOR=("${OPT_MONITOR_PACMAN[@]}") ;;
    apt)    REQUIRED=("${PKGS_APT[@]}");    OPT_TRAY=("${OPT_TRAY_APT[@]}");    OPT_MONITOR=("${OPT_MONITOR_APT[@]}") ;;
    dnf)    REQUIRED=("${PKGS_DNF[@]}");    OPT_TRAY=("${OPT_TRAY_DNF[@]}");    OPT_MONITOR=("${OPT_MONITOR_DNF[@]}") ;;
    zypper) REQUIRED=("${PKGS_ZYPPER[@]}"); OPT_TRAY=("${OPT_TRAY_ZYPPER[@]}"); OPT_MONITOR=("${OPT_MONITOR_ZYPPER[@]}") ;;
esac

pkg_installed() {
    case "${PM}" in
        pacman) pacman -Qq "$1" >/dev/null 2>&1 ;;
        apt)
            dpkg-query -W -f='${Status}' "$1" 2>/dev/null | grep -q 'install ok installed' \
                || dpkg-query -W -f='${Status}' "${1}t64" 2>/dev/null | grep -q 'install ok installed' ;;
        dnf)    rpm -q "$1" >/dev/null 2>&1 ;;
        # zypper search exits 0 even with no results, so match an installed
        # row in its output instead of relying on the exit code.
        zypper)
            local out
            out="$(zypper -q se -i -x -n -- "$1" 2>/dev/null || true)"
            grep -q '^i' <<< "${out}" ;;
        *)      return 1 ;;
    esac
}

pm_install_cmd() {
    local family="$1"
    shift
    case "${family}" in
        pacman)   printf 'sudo pacman -S --needed -- %s' "$*" ;;
        apt)      printf 'sudo apt-get update && sudo apt-get install -y -- %s' "$*" ;;
        dnf)      printf 'sudo dnf install -y -- %s' "$*" ;;
        zypper)   printf 'sudo zypper --non-interactive install -- %s' "$*" ;;
    esac
}

warn_missing_optional() {
    local label="$1"
    shift
    [[ -n "${PM}" ]] || return 0
    [[ $# -gt 0 ]] || return 0
    local missing=() p
    for p in "$@"; do
        pkg_installed "${p}" || missing+=("${p}")
    done
    if [[ ${#missing[@]} -gt 0 ]]; then
        printf 'Warning: missing optional %s packages: %s\n' "${label}" "${missing[*]}"
        printf 'The rest of the install works without them; to add them, run:\n'
        printf '  %s\n' "$(pm_install_cmd "${PM}" "${missing[@]}")"
    fi
}

# --- Phase 1: system packages ----------------------------------------------

MISSING=()
if [[ -n "${PM}" ]]; then
    for p in "${REQUIRED[@]}"; do
        pkg_installed "${p}" || MISSING+=("${p}")
    done
fi

if [[ ${#MISSING[@]} -gt 0 ]]; then
    printf 'Missing required packages:\n'
    printf '  %s\n' "${MISSING[@]}"
    printf 'To install them, this script would run:\n'
    printf '  %s\n' "$(pm_install_cmd "${PM}" "${MISSING[@]}")"
    if [[ "${DRY_RUN}" -eq 1 ]]; then
        printf 'Dry run: not asking for sudo and not installing anything.\n'
    else
        printf 'This is the only step of ./install.sh that needs sudo.\n'
        printf 'Run the package command now? [y/N] '
        read -r answer || answer=''
        case "${answer}" in
            y|Y|yes|Yes|YES) ;;
            *) printf 'Stopped: install the packages listed above yourself, then re-run ./install.sh.\n'
               exit 1 ;;
        esac
        sudo -v || die 'sudo authentication failed; nothing was installed.'
        if ! {
            case "${PM}" in
                pacman) sudo pacman -S --needed --noconfirm -- "${MISSING[@]}" ;;
                apt)    sudo apt-get update && sudo apt-get install -y -- "${MISSING[@]}" ;;
                dnf)    sudo dnf install -y -- "${MISSING[@]}" ;;
                zypper) sudo zypper --non-interactive install -- "${MISSING[@]}" ;;
            esac
        }; then
            die 'package installation failed; install the packages listed above manually, then re-run ./install.sh.'
        fi
        printf 'Required packages installed.\n'
    fi
elif [[ -z "${PM}" ]]; then
    printf 'No supported package manager was found (pacman, apt, dnf, zypper), so the required packages cannot be checked.\n'
    printf 'Install them manually first, for example:\n'
    printf '  Arch Linux / CachyOS: %s\n' "$(pm_install_cmd pacman "${PKGS_PACMAN[@]}")"
    printf '  Debian / Ubuntu:      %s\n' "$(pm_install_cmd apt "${PKGS_APT[@]}")"
    printf '  Fedora:               %s\n' "$(pm_install_cmd dnf "${PKGS_DNF[@]}")"
    printf '  openSUSE:             %s\n' "$(pm_install_cmd zypper "${PKGS_ZYPPER[@]}")"
    if [[ "${DRY_RUN}" -eq 0 ]]; then
        die 'stopping before the renderer build; re-run ./install.sh once the packages are installed.'
    fi
else
    printf 'All required packages are already installed.\n'
fi

warn_missing_optional 'tray (only minimize-to-tray needs it)' "${OPT_TRAY[@]+"${OPT_TRAY[@]}"}"
warn_missing_optional 'monitor-discovery (kscreen-doctor/xrandr)' "${OPT_MONITOR[@]+"${OPT_MONITOR[@]}"}"

# --- Phase 2: renderer -----------------------------------------------------

find_renderer() {
    if command -v linux-wallpaperengine >/dev/null 2>&1; then
        command -v linux-wallpaperengine
    elif [[ -x "${RENDERER_LINK}" ]]; then
        printf '%s' "${RENDERER_LINK}"
    fi
}

RENDERER_PATH="$(find_renderer || true)"
RENDERER_BUILT=0

if [[ -n "${RENDERER_PATH}" ]]; then
    printf 'Renderer: already available at %s; skipping the build.\n' "${RENDERER_PATH}"
elif [[ "${DRY_RUN}" -eq 1 ]]; then
    printf 'Renderer: not found in PATH or %s. Planned steps (nothing will run):\n' "${HOME}/.local/bin"
    if [[ -d "${ROOT}/.git" ]]; then
        printf '  git -C %s submodule update --init --recursive --force\n' "${ROOT}"
    fi
    if [[ -f "${BUILD_DIR}/CMakeCache.txt" && ( -f "${BUILD_DIR}/Makefile" || -f "${BUILD_DIR}/build.ninja" ) ]]; then
        printf '  # build directory %s is already configured\n' "${BUILD_DIR}"
        printf '  # purge any 0-byte object files left by an interrupted build\n'
        printf '  cmake --build %s --parallel\n' "${BUILD_DIR}"
    else
        printf '  # downloads the Chromium Embedded Framework distribution (network needed)\n'
        printf '  cmake -S %s -B %s -DCMAKE_BUILD_TYPE=Release\n' "${ROOT}" "${BUILD_DIR}"
        printf '  cmake --build %s --parallel\n' "${BUILD_DIR}"
    fi
    printf '  cmake --install %s --prefix %s\n' "${BUILD_DIR}" "${RENDERER_PREFIX}"
    printf '  ln -s %s %s\n' "${RENDERER_BIN}" "${RENDERER_LINK}"
    printf '  # would write the marker %s\n' "${RENDERER_MARKER}"
else
    printf 'Renderer: not found; building it from this checkout.\n'
    if [[ -d "${ROOT}/.git" ]]; then
        printf 'Updating git submodules...\n'
        git -C "${ROOT}" submodule update --init --recursive --force
    fi
    if [[ -f "${BUILD_DIR}/CMakeCache.txt" && ( -f "${BUILD_DIR}/Makefile" || -f "${BUILD_DIR}/build.ninja" ) ]]; then
        printf 'Build directory %s is already configured; reusing it.\n' "${BUILD_DIR}"
    else
        printf 'Note: the CMake configure step downloads the matching Chromium Embedded Framework distribution; network access is required.\n'
        cmake -S "${ROOT}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
    fi
    # An interrupted build can leave 0-byte object files that GNU make then
    # treats as up to date, which breaks the final link with dozens of
    # undefined references. Purge them so this build recompiles them.
    find "${BUILD_DIR}" -type f \( -name '*.o' -o -name '*.o.d' \) -size 0 -delete 2>/dev/null || true
    cmake --build "${BUILD_DIR}" --parallel
    cmake --install "${BUILD_DIR}" --prefix "${RENDERER_PREFIX}"
    install -d "${HOME}/.local/bin"
    ln -sfn "${RENDERER_BIN}" "${RENDERER_LINK}"
    : > "${RENDERER_MARKER}"
    RENDERER_BUILT=1
    printf 'Renderer installed: %s -> %s\n' "${RENDERER_LINK}" "${RENDERER_BIN}"
fi

# --- Phase 3: desktop app --------------------------------------------------

if [[ "${DRY_RUN}" -eq 1 ]]; then
    printf 'Desktop app: would run: %s\n' "${ROOT}/app/install.sh"
    printf 'Dry run complete; nothing was changed.\n'
    exit 0
fi

printf 'Desktop app: installing with app/install.sh...\n'
"${ROOT}/app/install.sh"

# --- Success summary -------------------------------------------------------

printf '\nInstall complete.\n'
if [[ ${#MISSING[@]} -gt 0 ]]; then
    printf '  System packages: installed (sudo was needed for this step only).\n'
else
    printf '  System packages: already present.\n'
fi
if [[ "${RENDERER_BUILT}" -eq 1 ]]; then
    printf '  Renderer: built from this checkout and installed at %s\n' "${RENDERER_BIN}"
else
    printf '  Renderer: kept the existing installation at %s\n' "${RENDERER_PATH}"
fi
printf '  App: package, launcher, menu entry and %s installed for this user.\n' \
    'linux-wallpaperengine-app.service'
printf 'Launch "Linux Wallpaper Engine" from your app menu, or run: %s\n' \
    "${HOME}/.local/bin/linux-wallpaperengine-app"
printf 'Verify the background service with: systemctl --user status linux-wallpaperengine-app.service\n'
printf 'Remove everything later with: %s --uninstall\n' "${ROOT}/install.sh"
