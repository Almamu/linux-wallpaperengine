#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Install per-user KDE wallpaper restoration and event-driven selection tracking."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

SERVICE = 'linux-wallpaperengine-restore.service'


def run(*args):
    return subprocess.check_output(args, text=True, stderr=subprocess.STDOUT).strip()


def systemd_quote(value):
    value = str(value)
    if any(c in value for c in '\n\r\0'):
        raise ValueError('Paths must not contain control characters')
    return '"' + value.replace('\\', '\\\\').replace('"', '\\"').replace('%', '%%').replace('$', '$$') + '"'


def availability():
    if os.geteuid() == 0:
        return 'run as your desktop user, without sudo'
    if 'KDE' not in os.environ.get('XDG_CURRENT_DESKTOP', '').split(':'):
        return 'a logged-in KDE session is required'
    if os.environ.get('XDG_SESSION_TYPE') != 'wayland' and not os.environ.get('WAYLAND_DISPLAY'):
        return 'a KDE Wayland session is required'
    for command in ('systemctl', 'kscreen-doctor'):
        if not shutil.which(command):
            return f'{command} is required (systemd user services and KDE libkscreen tools)'
    try:
        if run('systemctl', '--user', 'show', 'plasma-workspace.target', '-p', 'LoadState', '--value') != 'loaded':
            return 'the Plasma systemd user-session target is unavailable'
    except subprocess.CalledProcessError:
        return 'the systemd user manager is unavailable'
    return None


def service_text(script, binary):
    return f'''[Unit]
Description=Restore and remember Linux Wallpaper Engine backgrounds
After=plasma-plasmashell.service
PartOf=graphical-session.target

[Service]
Type=exec
ExecStart={systemd_quote(sys.executable)} {systemd_quote(script)} --binary {systemd_quote(binary)}
Restart=on-failure
RestartSec=5
KillMode=mixed
TimeoutStopSec=10
Environment=PYTHONUNBUFFERED=1

[Install]
WantedBy=plasma-workspace.target
'''


def install(binary=None):
    source = Path(__file__).resolve().parent
    config = Path(os.environ.get('XDG_CONFIG_HOME', Path.home() / '.config'))
    preferences = config / 'linux-wallpaperengine/autostart.json'
    if binary is None and preferences.exists():
        binary = Path(json.loads(preferences.read_text())['binary'])
    if binary is None:
        # Works both from build/output/kde/autostart and from the source tree.
        candidates = [source.parent.parent / 'linux-wallpaperengine',
                      source.parent.parent / 'build/output/linux-wallpaperengine']
        found = shutil.which('linux-wallpaperengine')
        if found:
            candidates.append(Path(found))
        binary = next((p for p in candidates if p.is_file() and os.access(p, os.X_OK)), None)
    if binary is None or not binary.is_file() or not os.access(binary, os.X_OK):
        raise ValueError('Build the renderer first, or specify --binary /absolute/path/to/linux-wallpaperengine')
    binary = binary.absolute()
    destination = Path.home() / '.local/libexec/linux-wallpaperengine'
    script = destination / 'restore-wallpapers.py'
    unit = config / 'systemd/user' / SERVICE
    text = service_text(script, binary)  # Validate paths before writing anything.
    destination.mkdir(parents=True, exist_ok=True)
    temporary = destination / 'restore-wallpapers.py.new'
    shutil.copyfile(source / 'restore-wallpapers.py', temporary)
    temporary.chmod(0o700)
    temporary.replace(script)
    unit.parent.mkdir(parents=True, exist_ok=True)
    temporary = unit.with_suffix('.new')
    temporary.write_text(text)
    temporary.replace(unit)
    preferences.parent.mkdir(parents=True, exist_ok=True)
    temporary = preferences.with_suffix('.tmp')
    temporary.write_text(json.dumps({'binary': str(binary)}, indent=2) + '\n')
    temporary.replace(preferences)
    # The recorder uses a lock. An already running instance keeps its own state;
    # never clear it or restart the service (which could stop its wallpapers).
    print(run(sys.executable, str(script), '--binary', str(binary), '--snapshot'))
    run('systemctl', '--user', 'daemon-reload')
    run('systemctl', '--user', 'enable', SERVICE)
    run('systemctl', '--user', 'start', SERVICE)
    print('Wallpaper autostart enabled for KDE login. Existing selections are retained.')
    print('Configuration changes trigger snapshots; there is no periodic process scan.')
    print('An already running tracker loads updated code at the next login.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path)
    parser.add_argument('--if-supported', action='store_true', help='Skip gracefully outside a KDE Wayland/systemd session')
    args = parser.parse_args()
    reason = availability()
    if reason:
        if args.if_supported:
            print('Autostart setup skipped: ' + reason + '.')
            print('Run python3 kde/autostart/install.py from a supported desktop session after building.')
            return
        parser.error(reason)
    try:
        install(args.binary)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'Autostart setup failed: {error}\n')


if __name__ == '__main__':
    main()
