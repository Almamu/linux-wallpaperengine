"""Distance/height fog through stock shader helpers and live scene settings.

Run on an isolated display under /tmp/lwe-render-fix-build.lock. Requires Pillow.
The five bands probe before/start/midpoint/end/after each native default range.
"""
import argparse
from pathlib import Path
import struct
import tempfile

from test_scene_reflections import capture, material, prepare, texture, write_json
from test_scene_legacy_lighting import fixture as model_fixture
from test_particle_rotation import prepare as particle_fixture


VERTEX = """attribute vec2 a_TexCoord;
varying vec2 v_TexCoord;
void main() {
    v_TexCoord = a_TexCoord;
    gl_Position = vec4(a_TexCoord.x * 2.0 - 1.0, 1.0 - a_TexCoord.y * 2.0, 0, 1);
}
"""

FRAGMENT = """// [COMBO] {"combo":"FOG","default":1}
#include \"common.h\"
#include \"common_fog.h\"
varying vec2 v_TexCoord;
void main() {
    float band = mod(floor(v_TexCoord.x * 10.0), 5.0);
    float distance = band < 1.0 ? 0.0 : band * 2.0 - 1.0;
    float height = 3.0 - band * 2.0;
    vec2 state = CalculateFogPixelState(distance, height);
    vec3 color = ApplyFog(vec3(0, 0, 1), state);
    if (v_TexCoord.x > 0.5) color = CAST3(ApplyFogAlpha(1.0, state));
    gl_FragColor = vec4(color, 1);
}
"""


def scripted(initial, updated):
    return {"value": initial, "script": """let frames = 0;
export function update(value) {
    frames++;
    return frames > 3 ? %s : value;
}
""" % updated}


def cases():
    # Literal results from the native 1..5 and 1..-3 ranges: midpoint density
    # is 1/4; combined RGB applies height first, then distance. Alpha is 15/16.
    blue, red, green = (0, 0, 255), (255, 0, 0), (0, 255, 0)
    white, black = (255, 255, 255), (0, 0, 0)
    off = [blue] * 5
    alpha = [white, white, (239, 239, 239), black, black]
    distance = [blue, blue, (64, 0, 191), red, red]
    height = [blue, blue, (0, 64, 191), green, green]
    both = [blue, blue, (64, 48, 143), red, red]
    return [
        ("disabled-defaults", {}, off, [white] * 5, None),
        ("distance-defaults", {"fogdistance": True}, distance, alpha, None),
        ("height-defaults", {"fogheight": True}, height, alpha, None),
        ("height-then-distance", {"fogdistance": True, "fogheight": True}, both, alpha, None),
        ("script-enable-distance", {"fogdistance": scripted(False, "true")}, distance, alpha, None),
        ("script-disable-distance", {"fogdistance": scripted(True, "false")}, off, [white] * 5, None),
        ("script-enable-height", {"fogheight": scripted(False, "true")}, height, alpha, None),
        ("script-disable-height", {"fogheight": scripted(True, "false")}, off, [white] * 5, None),
        ("script-repeated-toggles", {"fogdistance": scripted(False, "frames < 7 || frames >= 11")},
         distance, alpha, None),
        ("script-exchange-types", {"fogdistance": scripted(True, "false"),
                                   "fogheight": scripted(False, "true")}, height, alpha, None),
        ("material-opt-out", {"fogdistance": True, "fogheight": True}, off, [white] * 5, 0),
        ("material-opt-in", {"fogdistance": True, "fogheight": True}, both, alpha, 1),
        ("script-ranges-colors-density", {
            "fogheight": True, "fogheightcolor": scripted("1 0 0", "new Vec3(0, 1, 0)"),
            "fogheightstart": scripted(0, "1"), "fogheightend": scripted(1, "-3"),
            "fogheightstartdensity": scripted(1, "0.25"),
            "fogheightenddensity": scripted(0, "0.75"),
        }, [(0, 64, 191), (0, 64, 191), (0, 96, 159), (0, 191, 64), (0, 191, 64)],
         [(239, 239, 239), (239, 239, 239), (219, 219, 219), (112, 112, 112), (112, 112, 112)], None),
    ]


def fixture(root, settings, fog_combo=None):
    scene = prepare(root, FRAGMENT)
    (root / "shaders/probe.vert").write_text(VERTEX)
    scene["general"].update(orthogonalprojection=None, fov=45, nearz=0.1, farz=100,
                            hdr=False, fogdistancecolor="1 0 0", fogheightcolor="0 1 0")
    scene["general"].update(settings)
    scene["camera"].update(eye="0 0 5")
    scene["objects"][0].update(origin="0 0 0", size="320 180")
    write_json(root / "models/probe.json", {"material": "materials/probe.json", "width": 320, "height": 180})
    render_material = material("probe", ["single"])
    if fog_combo is not None:
        render_material["passes"][0]["combos"] = {"FOG": fog_combo}
    write_json(root / "materials/probe.json", render_material)
    # Draw as a model so image-layer composition cannot resample a source-sized
    # effect target. Oversized world
    # bounds keep clip-space diagnostic geometry inside the native cull region.
    vertices = b"".join(struct.pack("<12f", x * 100, y * 100, 0, 0, 0, 1, 1, 0, 0, 1, u, v)
                        for x, y, u, v in ((-1, -1, 0, 1), (1, -1, 1, 1),
                                           (1, 1, 1, 0), (-1, 1, 0, 0)))
    indices = struct.pack("<6H", 0, 1, 2, 0, 2, 3)
    (root / "models/probe.mdl").write_bytes(
        b"MDLV0016\0" + struct.pack("<3I", 15, 1, 1) + b"materials/probe.json\0"
        + struct.pack("<3I", 0, 15, len(vertices)) + vertices + struct.pack("<I", len(indices)) + indices + b"\0")
    scene["objects"] = [{"id": 1, "name": "fog probe", "model": "models/probe.mdl", "origin": "0 0 0"}]
    write_json(root / "scene.json", scene)
    return scene


def check_image(name, image, rgb, alpha):
    results = []
    for label, offset, expected in (("RGB", 0, rgb), ("additive alpha", 5, alpha)):
        for band, want in enumerate(expected):
            actual = image.getpixel((round(image.width * (band + offset + 0.5) / 10), image.height // 2))
            passed = max(abs(a - b) for a, b in zip(actual, want)) <= 1
            results.append(dict(name=f"{name} {label} band{band}", actual=actual, expected=want, passed=passed))
            print("PASS" if passed else "FAIL", results[-1], flush=True)
    return results


def run(engine, root):
    results = []
    for name, settings, rgb, alpha, fog_combo in cases():
        probe = root / name
        scene = fixture(probe, settings, fog_combo)
        image, _ = capture(engine, probe, scene, ("--msaa", "off"))
        log = (probe / "engine.log").read_text()
        assert not any(error in log for error in ("SyntaxError", "ReferenceError", "TypeError",
                                                  "Failed to setup object", "GL_INVALID")), log
        results.extend(check_image(name, image, rgb, alpha))
    results.extend(check_geometry(engine, root))
    write_json(root / "checks.json", results)
    assert all(check["passed"] for check in results), "Scene fog regression failed"


def check_geometry(engine, root):
    results = []
    for kind in ("image", "modern-image", "model", "particle", "model-binding"):
        probe = root / ("stock-" + kind)
        settings = {"fogdistance": scripted(False, "true"), "fogdistancestart": 1,
                    "fogdistanceend": 5, "fogdistancecolor": "1 0 0"}
        if kind in ("image", "modern-image"):
            scene = fixture(probe, settings)
            if kind == "modern-image":
                scene["version"] = 3
            (probe / "shaders/probe.vert").unlink()
            (probe / "shaders/probe.frag").unlink()
            scene["camera"]["eye"] = "0 0 3"
            scene["objects"] = [{"id": 1, "name": "image fog probe", "image": "models/probe.json",
                                 "origin": "0 0 0", "size": "8 8", "color": "0 0 1"}]
            shader_material = material("genericimage4", ["single"])
            shader_material["passes"][0]["combos"] = {"LIGHTING": 0, "REFLECTION": 0}
            write_json(probe / "materials/probe.json", shader_material)
        elif kind == "model":
            scene = model_fixture(probe, {"light": "ldirectional"})
            scene["objects"] = scene["objects"][:1]
            scene["objects"][0]["color"] = "0 0 1"
            texture(probe / "materials/blue.tex", [(0, 0, 255)])
            shader_material = material("generic4", ["blue"])
            shader_material["passes"][0]["combos"] = {"LIGHTING": 0, "REFLECTION": 0}
            write_json(probe / "materials/plane.json", shader_material)
        elif kind == "model-binding":
            scene = fixture(probe, settings)
            (probe / "shaders/probe.frag").write_text('''
#if FOG_DIST
uniform mat3 g_NormalModelMatrix;
#endif
void main() {
    bool valid = false;
#if FOG_DIST
    valid = length(g_NormalModelMatrix[2]) > 0.9;
#endif
    gl_FragColor = valid ? vec4(0.25, 0, 0.75, 1) : vec4(0, 1, 0, 1);
}
''')
        else:
            scene = particle_fixture(probe)
            scene["camera"]["eye"] = "0 0 3"
            (probe / "shaders/genericparticle.frag").write_text('''
#include "common.h"
#include "common_fog.h"
varying vec2 v_TexCoord;
uniform vec3 g_ViewRight;
void main() {
    if (distance(v_TexCoord, vec2(0.5, 0.5)) > 0.3) discard;
    // This owner-supplied basis must survive the shader change even if it
    // was absent from the original linked program.
#if FOG_DIST
    if (length(g_ViewRight) < 0.9) { gl_FragColor = vec4(0, 1, 0, 1); return; }
#endif
    gl_FragColor = vec4(ApplyFog(vec3(0, 0, 1), CalculateFogPixelState(3.0, 0.0)), 1);
}
''')
        scene["general"].update(settings)
        image, _ = capture(engine, probe, scene, ("--msaa", "off"))
        actual = image.getpixel((160, 90))
        # The legacy image omits scene.version. The modern image's generated
        # presentation pass must pick up fog enabled by the scene script.
        expected = (0, 0, 255) if kind == "image" else (64, 0, 191)
        passed = max(abs(a - b) for a, b in zip(actual, expected)) <= 1
        results.append(dict(name=f"stock {kind} geometry after fog enable", actual=actual,
                            expected=expected, passed=passed))
        print("PASS" if passed else "FAIL", results[-1], flush=True)
    return results


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("engine", type=Path)
    parser.add_argument("--artifacts", type=Path)
    args = parser.parse_args()
    if args.artifacts:
        run(args.engine.resolve(), args.artifacts.resolve())
    else:
        with tempfile.TemporaryDirectory(prefix="lwe-scene-fog-") as directory:
            run(args.engine.resolve(), Path(directory))
