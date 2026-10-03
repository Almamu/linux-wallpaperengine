#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Build as the invoking user; elevate only package installation and the command link.
set -Eeuo pipefail

say() { printf '\n%s\n' "$*"; }
die() { printf 'Error: %s\n' "$*" >&2; exit 1; }
run() {
    printf ' +'; printf ' %q' "$@"; printf '\n'
    if (( ! dry_run )); then "$@"; fi
}
root_run() {
    if (( ! dry_run )); then
        command -v sudo >/dev/null || die 'sudo is required for this system installation step.'
    fi
    run sudo -- "$@"
}

usage() {
    cat <<'HELP'
Usage: ./build.sh [options]

Detect Linux distribution, install missing dependencies, initialize submodules,
build Release with CMake, and link /usr/local/bin/linux-wallpaperengine to the
executable in this checkout's build/output directory. Run as your normal user.

  --dry-run          Show the plan without downloads, installs, or file changes
  --jobs N           Parallel build jobs (default: 2)
  --build-dir PATH   Build directory (default: <repository>/build)
  --bin-dir PATH     Command-link directory (default: /usr/local/bin)
  --yes              Accept package-manager confirmations (including Arch upgrades)
  --skip-deps        Use an already prepared toolchain; skip package installation
  --no-link          Build without installing the command symlink
  --no-autostart     Skip the per-user KDE wallpaper autostart installation
  -h, --help         Show this help

sudo is requested only when needed. Package managers keep their confirmation
prompts unless --yes is supplied. On Arch, installing missing packages includes a full system upgrade
(pacman -Syu) to avoid an unsupported partial upgrade. No third-party package
repositories are enabled automatically. KDE desktop setup is described in README.
HELP
}

# Match ID first, then ID_LIKE, rather than whichever package manager happens
# to be on PATH (a developer machine may have several of them installed).
manager_for() {
    local token
    for token in "$@"; do
        case "$token" in
            debian|ubuntu|linuxmint|pop|neon) printf 'apt-get'; return;;
            fedora|nobara|rhel|centos|rocky|almalinux|ol) printf 'dnf'; return;;
            arch|manjaro|endeavouros|garuda|cachyos) printf 'pacman'; return;;
            opensuse*|suse|sles) printf 'zypper'; return;;
            void) printf 'xbps-install'; return;;
        esac
    done
    return 1
}

detect_platform() {
    [[ $(uname -s) == Linux ]] || die 'This script supports Linux only.'
    case $(uname -m) in
        x86_64|aarch64) ;;
        *) die 'The bundled CEF build requires x86_64 or aarch64 Linux.';;
    esac
    getconf GNU_LIBC_VERSION >/dev/null 2>&1 || die 'CEF requires glibc; Alpine and other musl systems are not supported.'
    local ID='' ID_LIKE='' PRETTY_NAME='' VERSION_ID=''
    if [[ -r /etc/os-release ]]; then
        # shellcheck disable=SC1091
        source /etc/os-release
    elif [[ -r /usr/lib/os-release ]]; then
        # shellcheck disable=SC1091
        source /usr/lib/os-release
    else
        die 'Cannot find os-release to identify this distribution.'
    fi
    distro=${PRETTY_NAME:-$ID}
    # Intentional whitespace splitting: ID_LIKE is an os-release word list.
    local -a relatives=()
    read -r -a relatives <<< "$ID_LIKE"
    manager=$(manager_for "$ID" "${relatives[@]}") || {
        if (( skip_deps )); then manager=none
        else die "Unsupported distribution: $distro. Prepare dependencies manually and use --skip-deps."; fi
    }
    if (( ! skip_deps )); then
        if [[ -e /run/ostree-booted || -e /usr/share/ublue-os || $ID == steamos ]]; then
            die 'This is an image-based/read-only distribution. Build in a mutable glibc development environment; host package installation is not supported.'
        fi
        if [[ $manager == dnf ]] && ! command -v dnf >/dev/null && command -v dnf5 >/dev/null; then manager=dnf5; fi
        command -v "$manager" >/dev/null || die "Expected package manager '$manager' is missing."
    fi
    say "Detected: $distro ($manager)"
}

package_list() {
    packages=()
    case "$manager" in
        apt-get)
            packages=(build-essential git cmake pkg-config python3 ca-certificates tar bzip2
                libgl-dev libegl-dev libglew-dev freeglut3-dev zlib1g-dev libsdl2-dev
                libmpv-dev liblz4-dev libavcodec-dev libavformat-dev libavutil-dev
                libswscale-dev libswresample-dev libpulse-dev libfreetype-dev libglm-dev
                libglfw3-dev libgmp-dev libdbus-1-dev libwayland-dev wayland-protocols
                libx11-dev libxrandr-dev libxxf86vm-dev libxinerama-dev libxcursor-dev libxi-dev
                libnss3 libnspr4 libatk1.0-dev libatk-bridge2.0-dev libcups2-dev
                libxcomposite-dev libxdamage-dev libgbm-dev libasound2-dev libpango1.0-dev
                libcairo2-dev libudev-dev libxkbcommon-dev)
            ;;
        dnf|dnf5|zypper)
            packages=(gcc gcc-c++ make git cmake python3 ca-certificates tar bzip2 glm-devel
                'pkgconfig(gl)' 'pkgconfig(egl)' 'pkgconfig(glew)' 'pkgconfig(glut)'
                'pkgconfig(zlib)' 'pkgconfig(sdl2)' 'pkgconfig(mpv)' 'pkgconfig(liblz4)'
                'pkgconfig(libavcodec)' 'pkgconfig(libavformat)' 'pkgconfig(libavutil)'
                'pkgconfig(libswscale)' 'pkgconfig(libswresample)' 'pkgconfig(libpulse)'
                'pkgconfig(freetype2)' 'pkgconfig(glfw3)' 'pkgconfig(gmpxx)' 'pkgconfig(dbus-1)'
                'pkgconfig(wayland-client)' 'pkgconfig(wayland-egl)' 'pkgconfig(wayland-cursor)'
                'pkgconfig(wayland-protocols)' 'pkgconfig(wayland-scanner)' 'pkgconfig(x11)' 'pkgconfig(xrandr)'
                'pkgconfig(xxf86vm)' 'pkgconfig(xinerama)' 'pkgconfig(xcursor)' 'pkgconfig(xi)'
                'pkgconfig(atk)' 'pkgconfig(atk-bridge-2.0)' 'pkgconfig(xcomposite)'
                'pkgconfig(xdamage)' 'pkgconfig(gbm)' 'pkgconfig(alsa)' 'pkgconfig(pango)'
                'pkgconfig(cairo)' 'pkgconfig(libudev)' 'pkgconfig(xkbcommon)'
                'libnss3.so()(64bit)' 'libnspr4.so()(64bit)' 'libcups.so.2()(64bit)')
            packages+=(pkgconf-pkg-config)
            ;;
        pacman)
            packages=(base-devel git cmake pkgconf python ca-certificates tar bzip2
                libglvnd glew freeglut zlib sdl2 mpv lz4 ffmpeg libpulse freetype2 glm glfw
                gmp dbus wayland wayland-protocols libx11 libxrandr libxxf86vm libxinerama
                libxcursor libxi nss nspr at-spi2-core libcups libxcomposite libxdamage
                mesa alsa-lib pango cairo systemd-libs libxkbcommon)
            ;;
        xbps-install)
            packages=(base-devel git cmake pkg-config python3 ca-certificates tar bzip2
                MesaLib-devel libglvnd-devel glew-devel libfreeglut-devel zlib-devel SDL2-devel mpv-devel
                liblz4-devel ffmpeg6-devel pulseaudio-devel freetype-devel glm glfw-devel
                gmpxx-devel dbus-devel wayland-devel wayland-protocols libX11-devel
                libXrandr-devel libXxf86vm-devel libXinerama-devel libXcursor-devel libXi-devel
                nss nspr atk at-spi2-atk libcups libXcomposite libXdamage libgbm alsa-lib
                pango cairo eudev-libudev libxkbcommon-devel)
            ;;
    esac
}

installed() {
    case "$manager" in
        apt-get) [[ $(dpkg-query -W -f='${Status}' "$1" 2>/dev/null) == 'install ok installed' ]];;
        dnf|dnf5|zypper) rpm -q --whatprovides "$1" >/dev/null 2>&1;;
        pacman) pacman -T "$1" >/dev/null 2>&1;;
        xbps-install) [[ $(xbps-query -p state "$1" 2>/dev/null) == installed ]];;
        *) return 1;;
    esac
}
install_dependencies() {
    (( ! skip_deps )) || { say 'Dependency installation skipped.'; return; }
    package_list
    local item
    local -a missing=() confirmation=()
    if (( assume_yes )); then confirmation=(-y); fi
    for item in "${packages[@]}"; do
        if ! installed "$item"; then missing+=("$item"); fi
    done
    if (( ${#missing[@]} == 0 )); then say 'All dependency packages are already installed.'; return; fi
    say "Missing dependency packages/providers: ${missing[*]}"
    case "$manager" in
        apt-get)
            root_run apt-get update
            root_run apt-get install "${confirmation[@]}" --no-install-recommends "${missing[@]}"
            ;;
        dnf|dnf5) root_run "$manager" install "${confirmation[@]}" "${missing[@]}";;
        pacman)
            say 'Arch requires a full upgrade when synchronizing repositories; review the pacman transaction.'
            confirmation=()
            if (( assume_yes )); then confirmation=(--noconfirm); fi
            root_run pacman -Syu "${confirmation[@]}" --needed "${missing[@]}"
            ;;
        zypper)
            root_run zypper refresh
            root_run zypper install "${confirmation[@]}" --no-recommends "${missing[@]}"
            ;;
        xbps-install) root_run xbps-install -S "${confirmation[@]}" "${missing[@]}";;
    esac
    if (( ! dry_run )); then
        for item in "${packages[@]}"; do
            installed "$item" || die "Dependency '$item' is still missing. Check enabled repositories and rerun."
        done
    fi
}

check_toolchain() {
    local tool
    for tool in git cmake make pkg-config python3 wayland-scanner; do
        command -v "$tool" >/dev/null || die "Required command '$tool' is missing."
    done
    local compiler=${CXX:-c++}
    command -v "$compiler" >/dev/null || die "C++ compiler '$compiler' is missing (CXX must name a single executable)."
    # Configure must not silently build without Wayland support.
    pkg-config --exists wayland-client wayland-cursor wayland-egl wayland-protocols egl ||
        die 'Wayland/EGL development files are missing; cannot build this KDE Wayland fork.'
    printf '%s\n' '#include <concepts>' '#include <ranges>' '#include <map>' \
        'int main(){std::map<int,int> m; for(auto x:m|std::views::values)(void)x; return m.contains(1);}' |
        "$compiler" -std=c++20 -x c++ -fsyntax-only - || die 'A compiler and standard library with C++20 support are required.'
}

install_link() {
    (( ! no_link )) || return 0
    local executable=$build_dir/output/linux-wallpaperengine
    local destination=$bin_dir/linux-wallpaperengine
    if [[ -e $destination && ! -L $destination ]]; then
        die "Refusing to replace a regular file or directory: $destination. Move it aside or use --no-link."
    fi
    if [[ -L $destination && $(readlink -f -- "$destination") == "$executable" ]]; then
        say "Command link already points to $executable"; return
    fi
    # Writable custom directories (e.g. ~/.local/bin) need no elevation.
    if [[ -w $bin_dir ]] || { [[ ! -e $bin_dir ]] && [[ -w $(dirname -- "$bin_dir") ]]; }; then
        run mkdir -p -- "$bin_dir"
        run ln -sfnT -- "$executable" "$destination"
    else
        root_run mkdir -p -- "$bin_dir"
        root_run ln -sfnT -- "$executable" "$destination"
    fi
}

main() {
    dry_run=0; skip_deps=0; no_link=0; no_autostart=0; assume_yes=0; jobs=2; manager=none
    local repo
    repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
    build_dir=$repo/build
    bin_dir=/usr/local/bin
    while (( $# )); do
        case "$1" in
            --dry-run) dry_run=1;;
            --yes) assume_yes=1;;
            --skip-deps) skip_deps=1;;
            --no-link) no_link=1;;
            --no-autostart) no_autostart=1;;
            --jobs|--build-dir|--bin-dir)
                (( $# >= 2 )) && [[ -n $2 && $2 != --* ]] || die "Missing value for $1"
                case "$1" in --jobs) jobs=$2;; --build-dir) build_dir=$2;; --bin-dir) bin_dir=$2;; esac
                shift;;
            -h|--help) usage; return;;
            *) die "Unknown option: $1 (see --help)";;
        esac
        shift
    done
    [[ $jobs =~ ^[1-9][0-9]*$ ]] || die '--jobs must be a positive integer.'
    if (( EUID == 0 && ! dry_run )); then die 'Run ./build.sh as your normal user, without sudo. It requests elevation only when needed.'; fi
    build_dir=$(realpath -m -- "$build_dir")
    bin_dir=$(realpath -m -- "$bin_dir")
    [[ $build_dir != "$repo" ]] || die 'Use a separate build directory, not the source directory.'
    detect_platform
    install_dependencies
    if (( ! dry_run )); then check_toolchain; fi
    run git -C "$repo" submodule sync --recursive
    run git -C "$repo" submodule update --init --recursive
    run cmake -S "$repo" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
    run cmake --build "$build_dir" --target linux-wallpaperengine --parallel "$jobs"
    if (( ! dry_run )); then
        [[ -x $build_dir/output/linux-wallpaperengine ]] || die 'Build completed without the expected executable.'
        # Resolve dependencies and test argument parsing without starting a wallpaper.
        say "Checking executable startup (--help)..."
        "$build_dir/output/linux-wallpaperengine" --help >/dev/null
    fi
    install_link
    if (( ! no_autostart )); then
        run python3 "$repo/kde/autostart/install.py" --binary "$build_dir/output/linux-wallpaperengine" --if-supported
    fi
    if (( dry_run )); then say 'Dry run complete. No packages, files or desktop settings were changed.'
    else
        say "Build ready: $build_dir/output/linux-wallpaperengine"
        say 'Keep the repository/build directory in place: the command is a symlink, and CEF needs its adjacent resources.'
        say "For KDE setup, use $build_dir/output/kde/install.py as described in README.md, then select Linux Wallpaper Engine (Desktop Integration)."
    fi
}

if [[ ${BASH_SOURCE[0]} == "$0" ]]; then
    trap 'printf "Build/setup failed at line %s. Resolve the error above and rerun; no success link is installed after a failed build.\n" "$LINENO" >&2' ERR
    main "$@"
fi
