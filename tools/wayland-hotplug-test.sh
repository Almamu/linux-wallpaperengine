#!/usr/bin/env bash
# End-to-end test for Wayland output hotplug (#438): runs linux-wallpaperengine on a headless sway,
# adds and unplugs outputs, and checks with screenshots that the wallpaper is drawn on each requested
# output that appears, including after every output is gone and a new one comes back.
#
# Needs: sway (headless backend), swaymsg, grim, dbus-daemon, python3, and Mesa's EGL for software
# rendering.
# Usage: tools/wayland-hotplug-test.sh [path/to/linux-wallpaperengine]

set -euo pipefail

ENGINE=$(realpath "${1:-build/output/linux-wallpaperengine}")
TIMEOUT=${HOTPLUG_TEST_TIMEOUT:-20}

for tool in sway swaymsg grim dbus-daemon python3 rg; do
    command -v "$tool" >/dev/null || { echo "missing: $tool" >&2; exit 2; }
done
[[ -x $ENGINE ]] || { echo "not an executable: $ENGINE" >&2; exit 2; }

WORK=$(mktemp -d)
SWAY_PID=""
ENGINE_PID=""
DBUS_PID=""

cleanup() {
    [[ -n $ENGINE_PID ]] && kill "$ENGINE_PID" 2>/dev/null || true
    [[ -n $SWAY_PID ]] && kill "$SWAY_PID" 2>/dev/null || true
    [[ -n $DBUS_PID ]] && kill "$DBUS_PID" 2>/dev/null || true
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
    tail -30 "$WORK/engine.log" >&2 || true
    echo "--- sway log (last 15 lines) ---" >&2
    tail -15 "$WORK/sway.log" >&2 || true
    exit 1
}

pass() { echo "ok: $*"; }

# Reports how the engine ended (exit code or signal) and fails.
engine_died() {
    local status=0
    wait "$ENGINE_PID" 2>/dev/null || status=$?
    ENGINE_PID=""
    if ((status > 128)); then
        fail "the engine was killed by signal $((status - 128)) ($(kill -l $((status - 128)) 2>/dev/null)) $1"
    fi
    fail "the engine exited with status $status $1"
}

# A private session: sway's sockets live in their own runtime dir, nothing leaks to or from the desktop.
export XDG_RUNTIME_DIR="$WORK/run"
mkdir -m 700 "$XDG_RUNTIME_DIR"
export XDG_CONFIG_HOME="$WORK/config" XDG_CACHE_HOME="$WORK/cache" XDG_DATA_HOME="$WORK/data" XDG_STATE_HOME="$WORK/state"
export WPE_CONTROL_SOCKET="$XDG_RUNTIME_DIR/wallpaper.sock" WPE_LOG_FILE=off
unset WAYLAND_DISPLAY DISPLAY SWAYSOCK HYPRLAND_INSTANCE_SIGNATURE I3SOCK

# Isolate the engine and compositor from the desktop's media players and other session services.
{ read -r DBUS_SESSION_BUS_ADDRESS && read -r DBUS_PID; } < <(dbus-daemon --session --fork --print-address=1 --print-pid=1)
export DBUS_SESSION_BUS_ADDRESS

cat >"$WORK/sway.conf" <<'EOF'
xwayland disable
output * resolution 320x240
EOF

# --unsupported-gpu: older sway refuses to start when the NVIDIA driver is loaded, even though a
# headless output with the pixman renderer never touches the GPU.
WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 WLR_RENDERER=pixman WLR_LIBINPUT_NO_DEVICES=1 \
    sway --unsupported-gpu -c "$WORK/sway.conf" >"$WORK/sway.log" 2>&1 &
SWAY_PID=$!

wait_for() { # seconds, description, command...
    local seconds=$1 what=$2
    shift 2
    local deadline=$((SECONDS + seconds))
    until "$@"; do
        ((SECONDS < deadline)) || fail "timed out waiting for $what"
        kill -0 "$SWAY_PID" 2>/dev/null || fail "sway exited"
        [[ -z $ENGINE_PID ]] || kill -0 "$ENGINE_PID" 2>/dev/null || engine_died "while waiting for $what"
        sleep 0.2
    done
}

sway_ready() {
    SWAYSOCK=$(compgen -G "$XDG_RUNTIME_DIR/sway-ipc.*.sock" | head -n1) || return 1
    WAYLAND_DISPLAY=$(cd "$XDG_RUNTIME_DIR" && compgen -G 'wayland-*' | rg -v '\.lock$' | head -n1) || return 1
    export SWAYSOCK WAYLAND_DISPLAY
    swaymsg -t get_outputs >/dev/null 2>&1
}
wait_for 10 "sway to start" sway_ready

outputs() { swaymsg -t get_outputs -r | python3 -c 'import json,sys; print(" ".join(o["name"] for o in json.load(sys.stdin)))'; }

# An asset-free scene whose clear color is easy to check on screen.
PROJECT="$WORK/project"
mkdir "$PROJECT"
printf '{ "title": "hotplug test", "type": "scene", "file": "scene.json" }\n' >"$PROJECT/project.json"
cat >"$PROJECT/scene.json" <<'EOF'
{
    "camera": {"eye": "0 0 1", "center": "0 0 0", "up": "0 1 0"},
    "general": {"orthogonalprojection": {"width": 320, "height": 240}, "clearcolor": "1 0 0", "bloom": false},
    "objects": []
}
EOF
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
if len(pixels) < i + 3:
    sys.exit(1)
r, g, b = pixels[i:i+3]
sys.exit(0 if r > 180 and g < 80 and b < 80 else 1)'
}

engine_alive() { kill -0 "$ENGINE_PID" 2>/dev/null; }

# Use Mesa software rendering so the regression can also run without a display GPU.
MESA_EGL=$(compgen -G '/usr/share/glvnd/egl_vendor.d/*mesa*.json' | head -n1 || true)

start_output=$(outputs)
[[ $start_output == HEADLESS-1 ]] || fail "expected a single HEADLESS-1 output, got: $start_output"

# Both span members are absent initially. The first connection must build the
# group's wallpaper, and the second must recover it after all outputs vanish.
(
    cd "$WORK"
    exec env XDG_SESSION_TYPE=wayland XDG_CURRENT_DESKTOP=sway SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 \
        ${MESA_EGL:+__EGL_VENDOR_LIBRARY_FILENAMES=$MESA_EGL} \
        "$ENGINE" --silent --noautomute --no-audio-processing --no-fullscreen-pause --layer background --fps 10 --assets-dir "$WORK/assets" \
        --screen-root HEADLESS-1 --bg "$PROJECT" \
        --screen-span HEADLESS-2,HEADLESS-3 --bg "$PROJECT"
) >"$WORK/engine.log" 2>&1 &
ENGINE_PID=$!

wait_for "$TIMEOUT" "the wallpaper on HEADLESS-1" drawn HEADLESS-1
pass "wallpaper drawn on the output present at startup"

swaymsg -q create_output
wait_for "$TIMEOUT" "the wallpaper on HEADLESS-2 (added after startup)" drawn HEADLESS-2
pass "span wallpaper drawn when its first output was added after startup"

swaymsg -q output HEADLESS-1 unplug
wait_for 5 "the engine to log HEADLESS-1's removal" rg -Fq "Output HEADLESS-1 removed" "$WORK/engine.log"
engine_alive || engine_died "after HEADLESS-1 was unplugged"
drawn HEADLESS-2 || fail "HEADLESS-2 stopped showing the wallpaper after HEADLESS-1 was unplugged"
pass "other outputs keep drawing when one is unplugged"

# The single-monitor case from #438: every output goes away, then one comes back.
swaymsg -q output HEADLESS-2 unplug
wait_for 5 "the engine to log HEADLESS-2's removal" rg -Fq "Output HEADLESS-2 removed" "$WORK/engine.log"
sleep 1
engine_alive || engine_died "once no output was left"
pass "the engine keeps running with no outputs"

swaymsg -q create_output
wait_for "$TIMEOUT" "the wallpaper on HEADLESS-3 (after all outputs were gone)" drawn HEADLESS-3
pass "wallpaper drawn again on an output that appeared after all were gone"

# Stopping must work with no output left too: nothing is drawn then, so nothing wakes the event loop.
swaymsg -q output HEADLESS-3 unplug
wait_for 5 "the engine to log HEADLESS-3's removal" rg -Fq "Output HEADLESS-3 removed" "$WORK/engine.log"
# let it settle into waiting for events, which is where a stop request used to go unnoticed
sleep 2

kill -TERM "$ENGINE_PID"
for _ in $(seq 50); do
    kill -0 "$ENGINE_PID" 2>/dev/null || break
    sleep 0.2
done
if kill -0 "$ENGINE_PID" 2>/dev/null; then
    kill -KILL "$ENGINE_PID"
    fail "the engine did not exit within 10 seconds of SIGTERM (with no outputs left)"
fi
status=0
wait "$ENGINE_PID" || status=$?
ENGINE_PID=""
# the engine handles SIGTERM itself; 143 would mean it died from the signal instead of shutting down
((status == 0)) || fail "the engine exited with status $status on SIGTERM"
pass "clean shutdown with no outputs left"

echo "all hotplug checks passed"
