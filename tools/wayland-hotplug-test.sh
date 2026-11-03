#!/usr/bin/env bash
# End-to-end test for Wayland output hotplug (#438): runs linux-wallpaperengine on a headless sway,
# adds and unplugs outputs, and checks with screenshots that the wallpaper is drawn on each requested
# output that appears, including after every output is gone and a new one comes back.
#
# Needs: sway (headless backend), swaymsg, grim, ffmpeg, python3, and Mesa's EGL for software rendering.
# Usage: tools/wayland-hotplug-test.sh [path/to/linux-wallpaperengine]

set -euo pipefail

ENGINE=$(realpath "${1:-build/output/linux-wallpaperengine}")
TIMEOUT=${HOTPLUG_TEST_TIMEOUT:-20}

for tool in sway swaymsg grim ffmpeg python3; do
    command -v "$tool" >/dev/null || { echo "missing: $tool" >&2; exit 2; }
done
[[ -x $ENGINE ]] || { echo "not an executable: $ENGINE" >&2; exit 2; }

WORK=$(mktemp -d)
SWAY_PID=""
ENGINE_PID=""

cleanup() {
    [[ -n $ENGINE_PID ]] && kill "$ENGINE_PID" 2>/dev/null || true
    [[ -n $SWAY_PID ]] && kill "$SWAY_PID" 2>/dev/null || true
    wait 2>/dev/null || true
    if [[ ${KEEP_WORK:-0} == 1 ]]; then
        echo "logs kept in $WORK"
    else
        rm -rf "$WORK"
    fi
}
trap cleanup EXIT

fail() {
    echo "FAIL: $*" >&2
    echo "--- engine log (last 30 lines) ---" >&2
    grep -vE 'Resolving require module' "$WORK/engine.log" 2>/dev/null | tail -30 >&2 || true
    echo "--- sway log (last 15 lines) ---" >&2
    tail -15 "$WORK/sway.log" >&2 || true
    exit 1
}

pass() { echo "ok: $*"; }

# A private session: sway's sockets live in their own runtime dir, nothing leaks to or from the desktop.
export XDG_RUNTIME_DIR="$WORK/run"
mkdir -m 700 "$XDG_RUNTIME_DIR"
unset WAYLAND_DISPLAY DISPLAY SWAYSOCK HYPRLAND_INSTANCE_SIGNATURE I3SOCK

cat >"$WORK/sway.conf" <<'EOF'
xwayland disable
output * resolution 320x240
EOF

WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_LIBINPUT_NO_DEVICES=1 \
    sway -c "$WORK/sway.conf" >"$WORK/sway.log" 2>&1 &
SWAY_PID=$!

wait_for() { # seconds, description, command...
    local seconds=$1 what=$2
    shift 2
    local deadline=$((SECONDS + seconds))
    until "$@"; do
        ((SECONDS < deadline)) || fail "timed out waiting for $what"
        kill -0 "$SWAY_PID" 2>/dev/null || fail "sway exited"
        [[ -z $ENGINE_PID ]] || kill -0 "$ENGINE_PID" 2>/dev/null || fail "the engine exited while waiting for $what"
        sleep 0.2
    done
}

sway_ready() {
    SWAYSOCK=$(compgen -G "$XDG_RUNTIME_DIR/sway-ipc.*.sock" | head -n1) || return 1
    WAYLAND_DISPLAY=$(cd "$XDG_RUNTIME_DIR" && compgen -G 'wayland-*' | grep -v '\.lock$' | head -n1) || return 1
    export SWAYSOCK WAYLAND_DISPLAY
    swaymsg -t get_outputs >/dev/null 2>&1
}
wait_for 10 "sway to start" sway_ready

outputs() { swaymsg -t get_outputs -r | python3 -c 'import json,sys; print(" ".join(o["name"] for o in json.load(sys.stdin)))'; }

# A tiny video wallpaper: needs no Wallpaper Engine assets, and its color is easy to check on screen.
PROJECT="$WORK/project"
mkdir "$PROJECT"
ffmpeg -loglevel error -f lavfi -i color=c=red:s=320x240:r=10 -t 2 -pix_fmt yuv420p "$PROJECT/red.mp4"
printf '{ "title": "hotplug test", "type": "video", "file": "red.mp4" }\n' >"$PROJECT/project.json"
mkdir "$WORK/assets"

# Whether the center of an output shows the wallpaper (red).
drawn() {
    grim -o "$1" -t ppm - 2>/dev/null | python3 -c '
import re, sys
data = sys.stdin.buffer.read()
# binary PPM: "P6", width, height, maxval, then exactly one whitespace byte before the pixels
header = re.match(rb"P6\s+(\d+)\s+(\d+)\s+(\d+)\s", data)
if not header:
    sys.exit(1)
width, height = int(header.group(1)), int(header.group(2))
pixels = data[header.end():]
i = ((height // 2) * width + width // 2) * 3
r, g, b = pixels[i], pixels[i + 1], pixels[i + 2]
sys.exit(0 if r > 180 and g < 80 and b < 80 else 1)'
}

engine_alive() { kill -0 "$ENGINE_PID" 2>/dev/null; }

# Software rendering through Mesa, so this also runs on machines without a GPU (CI). --hwdec no keeps mpv
# from probing Vulkan hardware decoding, which can crash in the Vulkan loader under software rendering.
MESA_EGL=$(compgen -G '/usr/share/glvnd/egl_vendor.d/*mesa*.json' | head -n1 || true)

start_output=$(outputs)
[[ $start_output == HEADLESS-1 ]] || fail "expected a single HEADLESS-1 output, got: $start_output"

# HEADLESS-2 and -3 don't exist yet: headless outputs are numbered as they are created.
(
    cd "$WORK"
    exec env SDL_AUDIO_DRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 ${MESA_EGL:+__EGL_VENDOR_LIBRARY_FILENAMES=$MESA_EGL} \
        "$ENGINE" --silent --noautomute --no-audio-processing --fps 10 --hwdec no --assets-dir "$WORK/assets" \
        --screen-root HEADLESS-1 --bg "$PROJECT" \
        --screen-root HEADLESS-2 --bg "$PROJECT" \
        --screen-root HEADLESS-3 --bg "$PROJECT"
) >"$WORK/engine.log" 2>&1 &
ENGINE_PID=$!

wait_for "$TIMEOUT" "the wallpaper on HEADLESS-1" drawn HEADLESS-1
pass "wallpaper drawn on the output present at startup"

swaymsg -q create_output
wait_for "$TIMEOUT" "the wallpaper on HEADLESS-2 (added after startup)" drawn HEADLESS-2
pass "wallpaper drawn on an output added after startup"

swaymsg -q output HEADLESS-1 unplug
wait_for 5 "the engine to log HEADLESS-1's removal" grep -q "Output HEADLESS-1 removed" "$WORK/engine.log"
engine_alive || fail "the engine exited after HEADLESS-1 was unplugged"
drawn HEADLESS-2 || fail "HEADLESS-2 stopped showing the wallpaper after HEADLESS-1 was unplugged"
pass "other outputs keep drawing when one is unplugged"

# The single-monitor case from #438: every output goes away, then one comes back.
swaymsg -q output HEADLESS-2 unplug
wait_for 5 "the engine to log HEADLESS-2's removal" grep -q "Output HEADLESS-2 removed" "$WORK/engine.log"
sleep 1
engine_alive || fail "the engine exited once no output was left"
pass "the engine keeps running with no outputs"

swaymsg -q create_output
wait_for "$TIMEOUT" "the wallpaper on HEADLESS-3 (after all outputs were gone)" drawn HEADLESS-3
pass "wallpaper drawn again on an output that appeared after all were gone"

kill -TERM "$ENGINE_PID"
# give it 10 seconds, then kill it so a hang shows up as a failure instead of a stuck job
(sleep 10 && kill -KILL "$ENGINE_PID" 2>/dev/null) &
watchdog=$!
status=0
wait "$ENGINE_PID" || status=$?
kill "$watchdog" 2>/dev/null || true
ENGINE_PID=""
((status != 137)) || fail "the engine did not exit within 10 seconds of SIGTERM"
((status == 0 || status == 143)) || fail "the engine exited with status $status on SIGTERM"
pass "clean shutdown"

echo "all hotplug checks passed"
