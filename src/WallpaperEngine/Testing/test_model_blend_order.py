"""Overlapping MDLV surfaces must retain the native blended material order.

Run under flock /tmp/lwe-render-fix-build.lock; requires Pillow and a desktop.
"""
import argparse
import json
from pathlib import Path
import struct
import tempfile

from test_scene_reflections import capture
from test_scene_resolution import write_json


def fixture(root, order, sorting=False, custom=False, coplanar=False):
    root.mkdir(parents=True)
    write_json(root / "project.json", {"title": "Model blend order regression",
                                       "type": "scene", "file": "scene.json"})
    surfaces = {"body": ("normal", "0.2, 0.1, 0, 1"),
                "lamp": ("additive", "0, 0.4, 0, 1"),
                "glass": ("translucent", "0, 0, 0.2, 0.5"),
                "detail": ("additive", "0.1, 0, 0, 1")}
    (root / "shaders").mkdir()
    model = b"MDLV0013\0" + struct.pack("<3I", 15, 1, len(order))
    for name in order:
        blend, color = surfaces[name]
        write_json(root / f"materials/{name}.json", {"passes": [{
            "shader": name, "blending": blend, "cullmode": "nocull",
            "depthtest": "enabled", "depthwrite": "enabled"}]})
        (root / f"shaders/{name}.vert").write_text("""
uniform mat4 g_ModelViewProjectionMatrix;
attribute vec3 a_Position;
void main() { gl_Position = mul(vec4(a_Position, 1), g_ModelViewProjectionMatrix); }
""")
        (root / f"shaders/{name}.frag").write_text(
            f"void main() {{ gl_FragColor = vec4({color}); }}")
    indices = struct.pack("<6H", 0, 1, 2, 0, 2, 3)
    for name in order:
        # Separate blend-order checks from the equal-depth overlay regression.
        z = 0 if coplanar or name == "body" else 0.1
        vertices = b"".join(struct.pack("<12f", x, y, z, 0, 0, 1, 1, 0, 0, 1, 0, 0)
                            for x, y in ((-1, -1), (1, -1), (1, 1), (-1, 1)))
        model += (f"materials/{name}.json".encode() + b"\0"
                  + struct.pack("<2I", 0, len(vertices)) + vertices
                  + struct.pack("<I", len(indices)) + indices)
    (root / "models").mkdir()
    (root / "models/probe.mdl").write_bytes(model)
    return {"camera": {"eye": "0 0 3", "center": "0 0 0", "up": "0 1 0"},
            "general": {"orthogonalprojection": None, "nearz": 0.1, "farz": 100, "fov": 45,
                        "clearcolor": "0 0 0", "clearenabled": True, "bloom": False,
                        "transparentsorting": sorting, "customsortorder": custom},
            "objects": [{"id": 1, "name": "overlapping surfaces", "model": "models/probe.mdl"}]}


def run(engine, root):
    results = []
    # Body starts between blended entries: it must still render first. Additive
    # entries separated by glass expose regrouping when scene sorting is enabled.
    order = ["lamp", "body", "glass", "detail"]
    cases = [("lamp-under-glass", order, False, False, False, (51, 64, 26)),
             ("lamp-over-glass", ["glass", "body", "lamp", "detail"], False, False, False, (51, 115, 26)),
             ("automatic-scene-sorting", order, True, False, False, (51, 64, 26)),
             ("custom-scene-order", order, True, True, False, (51, 64, 26)),
             ("coplanar-lamps-do-not-add-color-twice", order, False, False, True, (51, 26, 0)),
             ("coplanar-lamps-with-scene-sorting", order, True, False, True, (51, 26, 0))]
    for name, entries, sorting, custom, coplanar, expected in cases:
        probe = root / name
        scene = fixture(probe, entries, sorting, custom, coplanar)
        image, _ = capture(engine, probe, scene)
        log = (probe / "engine.log").read_text()
        assert "Failed to setup object" not in log, log
        actual = image.getpixel((160, 90))
        passed = max(abs(a - b) for a, b in zip(actual, expected)) <= 1
        results.append({"name": name, "actual": actual, "expected": expected, "passed": passed})
        print(f"{'PASS' if passed else 'FAIL'} {name}: {actual}, expected {expected}", flush=True)
    (root / "checks.json").write_text(json.dumps(results, indent=2))
    assert all(result["passed"] for result in results), "Model blend-order regression"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("engine", type=Path)
    parser.add_argument("--artifacts", type=Path)
    args = parser.parse_args()
    if args.artifacts:
        run(args.engine.resolve(), args.artifacts.resolve())
    else:
        with tempfile.TemporaryDirectory(prefix="lwe-model-blend-order-") as directory:
            run(args.engine.resolve(), Path(directory))
