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
    # more of it under a wrapper, which adds e.g. a backtrace
    local lines=30
    [[ -n ${HOTPLUG_TEST_WRAPPER:-} ]] && lines=400
    echo "--- engine log (last $lines lines) ---" >&2
    grep -vE 'Resolving require module' "$WORK/engine.log" 2>/dev/null | tail -"$lines" >&2 || true
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
        [[ -z $ENGINE_PID ]] || kill -0 "$ENGINE_PID" 2>/dev/null || engine_died "while waiting for $what"
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

# HOTPLUG_TEST_WRAPPER runs the engine under another program (split on spaces, no quoting), e.g. to get
# a backtrace of a crash:
#   HOTPLUG_TEST_WRAPPER="gdb -q -batch -ex run -ex bt --args"
read -r -a WRAPPER <<<"${HOTPLUG_TEST_WRAPPER:-}"

# HEADLESS-2 and -3 don't exist yet: headless outputs are numbered as they are created.
(
    cd "$WORK"
    exec env SDL_AUDIO_DRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 ${MESA_EGL:+__EGL_VENDOR_LIBRARY_FILENAMES=$MESA_EGL} \
        "${WRAPPER[@]}" "$ENGINE" --silent --noautomute --no-audio-processing --fps 10 --hwdec no --assets-dir "$WORK/assets" \
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
engine_alive || engine_died "after HEADLESS-1 was unplugged"
drawn HEADLESS-2 || fail "HEADLESS-2 stopped showing the wallpaper after HEADLESS-1 was unplugged"
pass "other outputs keep drawing when one is unplugged"

# The single-monitor case from #438: every output goes away, then one comes back.
swaymsg -q output HEADLESS-2 unplug
wait_for 5 "the engine to log HEADLESS-2's removal" grep -q "Output HEADLESS-2 removed" "$WORK/engine.log"
sleep 1
engine_alive || engine_died "once no output was left"
pass "the engine keeps running with no outputs"

swaymsg -q create_output
wait_for "$TIMEOUT" "the wallpaper on HEADLESS-3 (after all outputs were gone)" drawn HEADLESS-3
pass "wallpaper drawn again on an output that appeared after all were gone"

# Stopping must work with no output left too: nothing is drawn then, so nothing wakes the event loop.
swaymsg -q output HEADLESS-3 unplug
wait_for 5 "the engine to log HEADLESS-3's removal" grep -q "Output HEADLESS-3 removed" "$WORK/engine.log"
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
