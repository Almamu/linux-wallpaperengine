"""Reflection input regressions using real textures and GPU draws; requires Pillow/desktop.

flock /tmp/lwe-render-fix-build.lock python3 \
    src/WallpaperEngine/Testing/test_scene_reflections.py build-release/output/linux-wallpaperengine
Use --artifacts /tmp/name to retain input scenes, captures, fbostats and logs.
"""
import argparse
import json
import os
from pathlib import Path
import re
import signal
import socket
import struct
import subprocess
import tempfile
import time

from PIL import Image

from test_scene_resolution import VERTEX, write_json


GREEN = (0, 255, 0)


def texture(path, colors):
    """TEXB0001 RGBA8 with deliberately distinct authored mip levels."""
    width = 1 << (len(colors) - 1)
    payload = (b"TEXV0005\0TEXI0001\0" + struct.pack("<7I", 0, 2, width, width, width, width, 0)
               + b"TEXB0001\0" + struct.pack("<2I", 1, len(colors)))
    for level, color in enumerate(colors):
        size = width >> level
        pixels = bytes((*color, 255)) * (size * size)
        payload += struct.pack("<3I", size, size, len(pixels)) + pixels
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(payload)


def prepare(root, fragment):
    root.mkdir(parents=True)
    (root / "shaders").mkdir()
    (root / "shaders/probe.vert").write_text(VERTEX)
    (root / "shaders/probe.frag").write_text(fragment)
    write_json(root / "project.json", {"title": "Reflection input regression", "type": "scene",
                                       "file": "scene.json"})
    texture(root / "materials/four.tex", [(255, 0, 0), (255, 255, 0), (0, 255, 255), (0, 0, 255)])
    texture(root / "materials/three.tex", [(255, 0, 0), (0, 0, 255), (0, 255, 0)])
    texture(root / "materials/single.tex", [(255, 255, 255)])
    write_json(root / "models/probe.json", {"material": "materials/probe.json", "width": 8, "height": 8})
    scene = {
        "camera": {"eye": "0 0 1", "center": "0 0 0", "up": "0 1 0"},
        "general": {"orthogonalprojection": {"width": 64, "height": 40},
                    "clearcolor": "0 0 0", "clearenabled": True, "bloom": False},
        "objects": [{"id": 1, "name": "probe", "image": "models/probe.json",
                     "origin": "32 20 0", "size": "64 40"}],
    }
    return scene


def material(shader, textures=None):
    result = {"shader": shader, "blending": "normal", "cullmode": "nocull",
              "depthtest": "disabled", "depthwrite": "disabled"}
    if textures is not None:
        result["textures"] = textures
    return {"passes": [result]}


def planar_mesh(root, name, points):
    vertices = b"".join(struct.pack("<12f", x, y, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0)
                        for x, y in points)
    indices = struct.pack("<3H", 0, 1, 2)
    (root / f"models/{name}.mdl").write_bytes(
        b"MDLV0013\0" + struct.pack("<3I", 15, 1, 1) + f"materials/{name}.json\0".encode()
        + struct.pack("<2I", 0, len(vertices)) + vertices + struct.pack("<I", len(indices)) + indices)


def capture(engine, root, scene, extra_args=()):
    write_json(root / "scene.json", scene)
    picture, control = root / "result.png", root / "control.sock"
    env = dict(os.environ, WPE_LOG_FILE=str(root / "engine.log"), WPE_CONTROL_SOCKET=str(control))
    with (root / "output.log").open("w") as log:
        process = subprocess.Popen([
            str(engine), "--window", "100x100x320x180", "--scaling", "stretch", "--fps", "15",
            "--volume", "0", "--screenshot", str(picture), "--screenshot-delay", "30", *extra_args, str(root)],
            env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
        try:
            deadline = time.monotonic() + 30
            while not picture.exists() and process.poll() is None and time.monotonic() < deadline:
                time.sleep(0.1)
            assert picture.exists(), (root / "output.log").read_text()
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
                client.settimeout(3)
                client.connect(str(control))
                client.sendall(b"fbostats\n")
                chunks = []
                while chunk := client.recv(65536):
                    chunks.append(chunk)
            stats = b"".join(chunks).decode()
            (root / "fbostats.txt").write_text(stats)
            with Image.open(picture) as image:
                result = image.convert("RGB")
        finally:
            if process.poll() is None:
                os.killpg(process.pid, signal.SIGINT)
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait()
    return result, stats


def run(engine, root, uniforms_only=False, inactive_reflections_only=False):
    results = []

    def check(name, actual, expected):
        passed = actual == expected
        results.append({"name": name, "actual": actual, "expected": expected, "passed": passed})
        print(f"{'PASS' if passed else 'FAIL'} {name}: {actual!r} (expected {expected!r})", flush=True)

    probe = root / "authored-levels"
    scene = prepare(probe, """
uniform sampler2D g_Texture0;
uniform float g_Texture0MipMapInfo;
uniform float g_Texture1MipMapInfo;
uniform float g_Texture9MipMapInfo;
uniform sampler2D g_Texture2; // {"hidden":true,"default":"_rt_Reflection"}
#if 0
uniform sampler2D g_Texture3; // {"hidden":true,"default":"_rt_MipMappedFrameBuffer"}
#endif
varying vec2 v_TexCoord;
void main() {
    float tile = floor(v_TexCoord.x * 4.0);
    bool valid;
    if (tile < 1.0) {
        valid = all(equal(texSample2DLod(g_Texture0, vec2(0.5), 0.0).rgb, vec3(1, 0, 0)))
            && all(equal(texSample2DLod(g_Texture0, vec2(0.5), 3.0).rgb, vec3(0, 0, 1)));
    } else if (tile < 2.0) valid = g_Texture0MipMapInfo == 4.0;
    else if (tile < 3.0) valid = g_Texture1MipMapInfo == 1.0;
    else valid = g_Texture9MipMapInfo == 1.0;
    gl_FragColor = vec4(valid ? vec3(0, 1, 0) : vec3(1, 0, 0), 1);
}
""")
    # Legacy generic2 declares a default reflection sampler unconditionally,
    # then compiles out its use. Both shader stages must ignore inactive defaults.
    (probe / "shaders/probe.vert").write_text(
        'uniform sampler2D g_Texture4; // {"hidden":true,"default":"_rt_Reflection"}\n' + VERTEX)
    write_json(probe / "materials/probe.json", material("probe", ["four", "single"]))
    image, stats = capture(engine, probe, scene)
    check("compiled-out reflection sampler does not allocate a snapshot",
          "_rt_MipMappedFrameBuffer@" in stats, False)
    check("inactive legacy reflection defaults are not resolved in either shader stage",
          "_rt_Reflection" in (probe / "engine.log").read_text(), False)
    for x, name in zip((40, 120, 200, 280), (
        "authored mip payload reaches explicit LOD0 and LOD3",
        "four-level texture reports count4 rather than maxLOD3",
        "single-level bound texture reports count1", "absent slot9 reports fallback1")):
        check(name, image.getpixel((x, 90)), GREEN)

    if inactive_reflections_only:
        (root / "checks.json").write_text(json.dumps(results, indent=2))
        assert all(result["passed"] for result in results), results
        return

    if not uniforms_only:
        # Sample an unchanged checkerboard region, far from this probe. After
        # warmup the answer is independent of previous-frame vs intra-frame
        # snapshot timing; this test deliberately makes no ordering assertion.
        probe = root / "snapshot-roughness"
        scene = prepare(probe, """
void main() {
    float value = 1.0 - mod(floor(gl_FragCoord.x) + floor(gl_FragCoord.y), 2.0);
    gl_FragColor = vec4(vec3(value), 1);
}
""")
        write_json(probe / "materials/probe.json", material("probe", ["four"]))
        (probe / "shaders/reflection.vert").write_text(VERTEX)
        (probe / "shaders/reflection.frag").write_text("""
uniform sampler2D g_Texture3; // {"hidden":true,"default":"_rt_MipMappedFrameBuffer"}
uniform float g_Texture3MipMapInfo;
varying vec2 v_TexCoord;
void main() {
    float tile = floor(v_TexCoord.x * 3.0);
    if (tile > 1.5) {
        gl_FragColor = vec4(g_Texture3MipMapInfo == 5.0 ? vec3(0, 1, 0) : vec3(1, 0, 0), 1);
    } else {
        float roughness = tile;
        vec2 sampleUV = vec2(64.5/320.0, 90.5/180.0);
        gl_FragColor = texSample2DLod(g_Texture3, sampleUV, roughness * g_Texture3MipMapInfo);
    }
}
""")
        reflection_material = material("reflection", ["four", None, None, "_rt_MipMappedFrameBuffer"])
        reflection_material["passes"][0]["combos"] = {"REFLECTION": 1}
        write_json(probe / "materials/reflection.json", reflection_material)
        write_json(probe / "models/reflection.json", {
            "material": "materials/reflection.json", "width": 8, "height": 8})
        scene["objects"].append({"id": 2, "name": "reflection probe", "image": "models/reflection.json",
                                 "origin": "48 20 0", "size": "24 16"})
        image, stats = capture(engine, probe, scene)
        match = re.search(r"(?:^|[ ;])_rt_MipMappedFrameBuffer@(\d+)x(\d+)x", stats)
        check("snapshot has a dedicated output-sized target rather than the main-FBO alias",
              tuple(map(int, match.groups())) if match else None, (320, 180))
        check("reflection roughness0 samples sharp checker detail", image.getpixel((200, 90)), (255, 255, 255))
        rough = image.getpixel((240, 90))
        check(f"reflection roughness1 averages checker detail (sample {rough})",
              all(abs(channel - 128) <= 2 for channel in rough), True)
        check("320x180 reflection target exposes native shortened mip count5",
              image.getpixel((280, 90)), GREEN)

        # Alternate a channel each update so current-frame, stale and previous-frame
        # inputs are distinguishable. The fixed blue channel detects snapshots
        # taken after synthetic bloom. Neither assertion depends on initial contents.
        probe = root / "snapshot-timing"
        scene = prepare(probe, """
uniform vec3 g_Color;
void main() { gl_FragColor = vec4(g_Color.x, 0, 0.25, 1); }
""")
        toggle = """let frame = 0;
export function update(value) {
    frame++;
    return new Vec3((frame % 2) * 0.5, 0, 0);
}"""
        scene["general"].update(bloom=True, bloomstrength=1, bloomthreshold=0)
        scene["objects"][0]["color"] = {"value": "0 0 0", "script": toggle}
        write_json(probe / "materials/probe.json", material("probe", ["four"]))
        (probe / "shaders/reflection.vert").write_text(VERTEX)
        (probe / "shaders/reflection.frag").write_text("""
uniform sampler2D g_Texture3; // {"hidden":true,"default":"_rt_MipMappedFrameBuffer"}
uniform vec3 g_Color;
varying vec2 v_TexCoord;
void main() {
    vec3 previous = texSample2DLod(g_Texture3, vec2(0.15, 0.5), 0.0).rgb;
    bool valid = v_TexCoord.x < 0.5
        ? abs(previous.r - (0.5 - g_Color.x)) < 0.01
        : abs(previous.b - 0.25) < 0.01;
    gl_FragColor = vec4(valid ? vec3(0, 1, 0) : vec3(1, 0, 0), 1);
}
""")
        write_json(probe / "materials/reflection.json", reflection_material)
        write_json(probe / "models/reflection.json", {
            "material": "materials/reflection.json", "width": 8, "height": 8})
        scene["objects"].append({"id": 2, "name": "reflection timing probe",
                                 "image": "models/reflection.json", "origin": "48 20 0",
                                 "size": "24 16", "color": {"value": "0 0 0", "script": toggle}})
        image, _ = capture(engine, probe, scene)
        for x, name in ((210, "reflection reads the previous completed scene"),
                        (270, "reflection snapshot excludes synthetic bloom")):
            pixel = image.getpixel((x, 90))
            check(f"{name} (sample {pixel})", pixel[1] > 240 and pixel[0] < 64, True)
        pixel = image.getpixel((48, 90))
        check(f"bloom exclusion fixture actually brightens blue (sample {pixel})", pixel[2] > 90, True)

    probe = root / "effect-bindings"
    scene = prepare(probe, """
uniform sampler2D g_Texture0;
varying vec2 v_TexCoord;
void main() { gl_FragColor = texSample2DLod(g_Texture0, v_TexCoord, 0.0); }
""")
    write_json(probe / "materials/probe.json", material("probe", ["four"]))
    (probe / "shaders/effect.vert").write_text(VERTEX)
    (probe / "shaders/effect.frag").write_text("""
uniform vec3 g_Screen;
uniform sampler2D g_Texture1;
uniform sampler2D g_Texture2;
uniform vec4 g_Texture0Resolution;
uniform float g_Texture0MipMapInfo;
uniform float g_Texture1MipMapInfo;
uniform float g_Texture2MipMapInfo;
varying vec2 v_TexCoord;
void main() {
    float tile = floor(v_TexCoord.x * 4.0);
    bool valid;
    if (tile < 1.0) valid = all(lessThan(abs(g_Screen - vec3(320, 180, 320.0/180.0)), vec3(0.0001)));
    else if (tile < 2.0) valid = g_Texture0MipMapInfo == 1.0
        && all(equal(g_Texture0Resolution, vec4(8)));
    else if (tile < 3.0) valid = g_Texture1MipMapInfo == 3.0
        && all(equal(texSample2DLod(g_Texture1, vec2(0.5), 2.0).rgb, vec3(0, 1, 0)));
    else valid = g_Texture2MipMapInfo == 1.0
        && all(equal(texSample2DLod(g_Texture2, vec2(0.5), 0.0).rgb, vec3(1, 0, 0)));
    gl_FragColor = vec4(valid ? vec3(0, 1, 0) : vec3(1, 0, 0), 1);
}
""")
    write_json(probe / "materials/effect.json", material("effect", [None, "single", "four"]))
    write_json(probe / "effects/probe/effect.json", {"passes": [{
        "material": "materials/effect.json", "bind": [{"index": 2, "name": "previous"}]}]})
    scene["objects"][0]["effects"] = [{"id": 2, "file": "effects/probe/effect.json",
                                      "passes": [{"id": 3, "textures": [None, "three"]}]}]
    image, _ = capture(engine, probe, scene)
    for x, name in zip((40, 120, 200, 280), (
        "8x8 effect receives output-sized g_Screen320x180",
        "effect input uses actual one-level FBO rather than original four-level source",
        "texture override reports its actual three-level chain", "previous bind reports actual one-level input")):
        check(name, image.getpixel((x, 90)), GREEN)

    (root / "checks.json").write_text(json.dumps(results, indent=2))
    failed = [result["name"] for result in results if not result["passed"]]
    assert not failed, f"{len(failed)} reflection regressions failed: {', '.join(failed)}"


def check_legacy_planar(engine, root):
    for name, options, active in (
        ("enabled", ("--render-scale", "1.5"), True),
        ("raw", (), True),
        ("positive", (), True),
        ("inverted", (), True),
        ("mixed", (), True),
        ("disabled", ("--reflection", "disabled"), True),
        ("inactive", (), False),
    ):
        directory = root / name
        uv_source = "v_UV" if name == "raw" else "projectedUV"
        if name == "positive":
            uv_source += " + vec2(0.0, 0.05)"
        if name == "inverted":
            uv_source = "vec2(projectedUV.x, 1.0 - projectedUV.y)"
        fragment = """
uniform sampler2D g_Texture1;
uniform vec4 g_Texture1Resolution;
uniform float g_Texture1MipMapInfo;
varying vec3 v_ScreenPos;
varying vec2 v_UV;
void main() {
    vec2 projectedUV = (v_ScreenPos.xy / v_ScreenPos.z) * 0.5 + 0.5;
    gl_FragColor = g_Texture1MipMapInfo == 1.0 && all(equal(g_Texture1Resolution, vec4(320, 180, 320, 180)))
        ? texSample2D(g_Texture1, UV_SOURCE) : vec4(1, 1, 0, 1);
}
""".replace("UV_SOURCE", uv_source)
        textures = ["single", "_rt_Reflection"]
        if name == "mixed":
            fragment = "uniform sampler2D g_Texture2;\n" + fragment.replace("gl_FragColor =", "vec4 reflectionColor =")
            fragment = fragment.rstrip().removesuffix("}") + """
    gl_FragColor = vec4(reflectionColor.r, texSample2D(g_Texture2, projectedUV).g, reflectionColor.b, 1.0);
}
"""
            textures.append("rows")
        scene = prepare(directory, fragment if active else "void main() { gl_FragColor = vec4(0.0, 0.0, 1.0, 1.0); }")
        if name == "mixed":
            rows = bytes((0, 255, 0, 255) * 2 + (0, 0, 0, 255) * 2)
            (directory / "materials/rows.tex").write_bytes(
                b"TEXV0005\0TEXI0001\0" + struct.pack("<7I", 0, 3, 2, 2, 2, 2, 0)
                + b"TEXB0001\0" + struct.pack("<5I", 1, 1, 2, 2, len(rows)) + rows)
        (directory / "shaders/probe.vert").write_text("""
attribute vec3 a_Position;
varying vec2 v_UV;
varying vec3 v_ScreenPos;
void main() {
    gl_Position = vec4(a_Position.xy, 0.5, 1.0);
    v_UV = a_Position.xy * 0.5 + 0.5;
    v_ScreenPos = gl_Position.xyw;
}
""")
        for shader, color in (("red", "1, 0, 0"), ("green", "0, 1, 0")):
            (directory / f"shaders/{shader}.vert").write_text(VERTEX)
            (directory / f"shaders/{shader}.frag").write_text(f"void main() {{ gl_FragColor = vec4({color}, 1); }}")
            marker = material(shader, ["single"])
            marker["passes"][0]["cullmode"] = "normal"
            write_json(directory / f"materials/{shader}.json", marker)
        write_json(directory / "materials/probe.json", material("probe", textures))

        for mesh, points in (
            ("red", [(-0.25, 0.43), (0.25, 0.43), (0.0, 0.85)]),
            ("green", [(0.52, 0.43), (1.0, 0.43), (0.76, 0.85)]),
            ("probe", [(-1.0, -1.0), (3.0, -1.0), (-1.0, 3.0)]),
        ):
            planar_mesh(directory, mesh, points)
        scene["camera"]["eye"] = "0 0 4"
        scene["general"].update(orthogonalprojection=None, fov=45, clearcolor="0.25 0.125 0.5")
        scene["objects"] = [
            {"id": 1, "name": "reflected marker", "model": "models/red.mdl"},
            {"id": 2, "name": "excluded marker", "model": "models/green.mdl", "reflected": False},
            {"id": 3, "name": "reflection consumer", "model": "models/probe.mdl"},
        ]
        picture, stats = capture(engine, directory, scene, ("--msaa", "off", *options))
        colors = list(picture.getdata())
        assert ("_rt_Reflection@" in stats) == active, (name, stats)
        if active:
            assert "_rt_Reflection@320x180x" in stats, (name, stats)
            if name != "mixed":
                assert all(abs(a - b) <= 1 for a, b in zip(picture.getpixel((50, 70)), (64, 32, 128))), name
        if name in ("enabled", "positive", "raw", "inverted", "mixed"):
            red_rows = [index // picture.width for index, color in enumerate(colors) if color == (255, 0, 0)]
            assert len(red_rows) > 100, (name, red_rows[:10])
            if name in ("raw", "inverted"):
                assert max(red_rows) < picture.height // 2, (name, max(red_rows))
            elif name == "positive":
                assert min(red_rows) >= 121, (name, min(red_rows))
            else:
                assert 112 <= min(red_rows) <= 114, (name, min(red_rows))
            if name == "mixed":
                assert picture.getpixel((50, 50))[1] == 255 and picture.getpixel((50, 130))[1] == 0
            assert GREEN not in colors, name
        elif name == "disabled":
            assert (255, 0, 0) not in colors and GREEN not in colors, name
        print(f"PASS legacy planar reflection {name}", flush=True)


def check_reflected_sprite_trails(engine, root):
    results = {}
    for steps in (4, 10):
        ordinary = {}
        for name in ("eligible", "excluded", "disabled"):
            directory = root / f"{name}-{steps}"
            scene = prepare(directory, """
uniform sampler2D g_Texture1;
varying vec3 v_ScreenPos;
void main() {
    vec2 uv = (v_ScreenPos.xy / v_ScreenPos.z) * 0.5 + 0.5;
    gl_FragColor = vec4(0, texSample2D(g_Texture1, uv).r, 0, 1);
}
""")
            (directory / "shaders/probe.vert").write_text("""
attribute vec3 a_Position;
varying vec3 v_ScreenPos;
void main() {
    gl_Position = vec4(a_Position.xy, 0.5, 1.0);
    v_ScreenPos = gl_Position.xyw;
}
""")
            write_json(directory / "materials/probe.json", material("probe", ["single", "_rt_Reflection"]))
            planar_mesh(directory, "probe", [(-1, -1), (3, -1), (-1, 3)])
            texture(directory / "materials/red.tex", [(255, 0, 0)])
            trail_pass = {
                "shader": "genericparticle", "textures": ["red"], "blending": "additive",
                "depthtest": "enabled", "depthwrite": "disabled", "cullmode": "nocull"}
            trail = {
                "maxcount": 1,
                "emitter": [{"name": "sphererandom", "instantaneous": 1, "rate": 0,
                             "distancemin": 0, "distancemax": 0}],
                "initializer": [{"name": "lifetimerandom", "min": 100, "max": 100},
                                {"name": "sizerandom", "min": 0.8, "max": 0.8},
                                {"name": "velocityrandom", "min": "1 0.2 0", "max": "1 0.2 0"}],
                "operator": [{"name": "movement", "drag": 0, "gravity": "0 0 0"}],
                "renderer": [{"name": "spritetrail", "length": 1}]}
            for culling in ("nocull", "normal"):
                path = f"materials/trail-{culling}.json"
                write_json(directory / path, {"passes": [dict(trail_pass, cullmode=culling)]})
                write_json(directory / f"particles/trail-{culling}.json", dict(trail, material=path))
            # Match the simulation interval across runs despite startup and frame-pacing jitter.
            timing = """let ticks = 0, steps = 0;
export function init(v) { thisLayer.instance.rate = 0; return v; }
export function update(v) {
    ++ticks;
    thisLayer.instance.rate = 0;
    if (ticks >= 4 && steps < STEPS && engine.frametime > 0 && engine.frametime < 0.095) {
        thisLayer.instance.rate = 0.05 / engine.frametime;
        ++steps;
    }
    return v;
}
""".replace("STEPS", str(steps))
            scene["camera"]["eye"] = "0 0 10"
            scene["general"].update(orthogonalprojection=None, nearz=0.1, fov=45, hdr=False)
            scene["objects"] = [{"id": 1, "name": "Reflection readback", "model": "models/probe.mdl"}]
            for identifier, culling, x in ((2, "nocull", -1), (3, "normal", 1)):
                scene["objects"].append({
                    "id": identifier, "name": f"Moving sprite trail ({culling})",
                    "particle": f"particles/trail-{culling}.json", "origin": f"{x} 1.5 0",
                    "reflected": name != "excluded", "visible": {"value": True, "script": timing},
                })
            options = ("--reflection", "disabled") if name == "disabled" else ()
            picture, stats = capture(engine, directory, scene, ("--msaa", "off", *options))
            assert "_rt_Reflection@320x180x" in stats, (name, stats)
            direct = picture.split()[0].point(lambda value: 255 if value > 192 else 0)
            reflected = picture.split()[1].point(lambda value: 255 if value > 192 else 0)
            assert direct.getbbox() is not None and direct.getbbox()[3] < 90, (name, direct.getbbox())
            ordinary[name] = direct.tobytes()
            results[name, steps] = (direct.crop((0, 0, 160, 180)).getbbox(), reflected.getbbox(),
                                    reflected.crop((160, 0, 320, 180)).getbbox())
        assert ordinary["eligible"] == ordinary["excluded"] == ordinary["disabled"], results
    assert results["eligible", 10][0][0] > results["eligible", 4][0][0] + 3, results
    for (name, steps), (direct, reflected, culled) in results.items():
        print(f"Sprite trail {name}, {steps} steps, ordinary bounds {direct}, reflection bounds {reflected}, "
              f"normal-culling reflection bounds {culled}", flush=True)
        if name == "eligible":
            assert reflected is not None and reflected[1] > 90, (name, reflected)
            assert culled is not None and culled[1] > 90, (name, culled)
        else:
            assert reflected is None, (name, reflected)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("engine", type=Path)
    parser.add_argument("--artifacts", type=Path)
    parser.add_argument("--uniforms-only", action="store_true")
    parser.add_argument("--inactive-reflections-only", action="store_true")
    tests = parser.add_mutually_exclusive_group()
    tests.add_argument("--legacy-planar-only", action="store_true")
    tests.add_argument("--legacy-particles-only", action="store_true")
    args = parser.parse_args()
    def run_checks(directory):
        if args.legacy_planar_only:
            check_legacy_planar(args.engine.resolve(), directory)
        elif args.legacy_particles_only:
            check_reflected_sprite_trails(args.engine.resolve(), directory)
        else:
            run(args.engine.resolve(), directory, args.uniforms_only, args.inactive_reflections_only)
    if args.artifacts:
        run_checks(args.artifacts.resolve())
    else:
        with tempfile.TemporaryDirectory(prefix="lwe-reflections-") as directory:
            run_checks(Path(directory))
