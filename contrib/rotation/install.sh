#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
BIN_DIR="${HOME}/.local/bin"
SYSTEMD_DIR="${HOME}/.config/systemd/user"

install -Dm755 "${ROOT}/contrib/rotation/wallpaper-engine-rotate.py" \
    "${BIN_DIR}/wallpaper-engine-rotate.py"
install -Dm644 "${ROOT}/contrib/rotation/linux-wallpaperengine-rotation.service" \
    "${SYSTEMD_DIR}/linux-wallpaperengine-rotation.service"

systemctl --user daemon-reload
systemctl --user enable linux-wallpaperengine-rotation.service
systemctl --user restart linux-wallpaperengine-rotation.service
echo "Wallpaper rotation enabled: one Workshop wallpaper every 10 minutes."
