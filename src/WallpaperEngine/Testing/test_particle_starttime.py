"""Render a held, nonrandom probe of authored particle prewarm steps."""
import argparse
import json
from pathlib import Path
import shutil
import tempfile

from test_particle_rotation import prepare
from test_scene_reflections import capture, write_json


def make_particle(start, capacity, *, speed=100, instantaneous=1):
    velocity = f"{speed} 0 0"
    return {
        "material": "materials/marker.json", "maxcount": capacity, "starttime": start,
        "emitter": [{"name": "sphererandom", "rate": 0, "instantaneous": instantaneous,
                     "distancemin": 0, "distancemax": 0}],
        "initializer": [
            {"name": "lifetimerandom", "min": 20, "max": 20},
            {"name": "sizerandom", "min": 12, "max": 12},
            {"name": "velocityrandom", "min": velocity, "max": velocity},
        ],
        "operator": [{"name": "movement", "gravity": "0 0 0", "drag": 0}],
        "renderer": [{"name": "sprite"}],
    }


def build_scene(root):
    scene = prepare(root, perspective=False)
    scene["general"]["orthogonalprojection"] = {"width": 320, "height": 180}
    scene["objects"] = []
    (root / "shaders/genericparticle.frag").write_text("void main() { gl_FragColor = vec4(1.0); }\n")
    hold = "export function init(v) { thisLayer.instance.rate = 0; return v; }"
    probes = []

    def layer(name, col, row, expected, script=hold, rate=.01):
        x, y = 20 + 100 * col, 30 + 60 * row
        probes.append((name, col, row, None if expected is None else x + expected))
        return {"id": len(probes), "name": name, "particle": f"particles/{name}.json",
                "origin": {"value": f"{x} {y} 0", "script": script}, "instanceoverride": {"rate": rate}}

    def definition(name, particle):
        write_json(root / f"particles/{name}.json", particle)

    for col, capacity, travel in ((0, 499, 25), (1, 500, 40)):
        name = f"capacity-{capacity}"
        definition(name, make_particle(.21, capacity))
        scene["objects"].append(layer(name, col, 0, travel))
    definition("zero", make_particle(0, 499))
    scene["objects"].append(layer("zero", 2, 0, None))

    definition("descendant", make_particle(.21, 500))
    parent = make_particle(.51, 499, instantaneous=0)
    parent["children"] = [{"name": "particles/descendant.json"}]
    definition("parent", parent)
    scene["objects"].append(layer("parent", 0, 1, 40))

    definition("stopped", make_particle(.21, 499))
    stop = (hold + "\nlet done = false; export function update(v) {"
            " if (!done && engine.runtime > .25) { done = true; thisLayer.stop(); thisLayer.play(); } return v; }")
    scene["objects"].append(layer("stopped", 1, 1, None, stop))

    definition("created", make_particle(.21, 499))
    created = layer("created", 2, 1, 25)
    script = ("let done = false; export function update(v) { if (!done && engine.runtime > .25) {"
              f" done = true; thisScene.createLayer({json.dumps(created)}); }} return v; }}")
    definition("display-control", make_particle(0, 1, speed=0))
    scene["objects"].append(layer("display-control", 0, 2, 0, script, rate=1))

    definition("negative", make_particle(-.21, 500))
    scene["objects"].append(layer("negative", 1, 2, None))
    # Float32 accumulation reaches 3 seconds after 61 ticks of .05f.
    definition("float-clock", make_particle(3, 499, speed=20))
    scene["objects"].append(layer("float-clock", 2, 2, 61))
    return scene, probes


def check_centers(image, probes):
    results = {}
    for name, col, row, expected in probes:
        left, top = 100 * col, 60 * (2 - row)
        pixels = image.crop((left, top, left + 100, top + 60))
        points = [left + x for y in range(pixels.height) for x in range(pixels.width)
                  if pixels.getpixel((x, y))[0] > 200]
        center = sum(points) / len(points) + .5 if points else None
        passed = center is None if expected is None else center is not None and abs(center - expected) <= .75
        results[name] = {"center_x": center, "expected_x": expected, "passed": passed}
        print(f"{'PASS' if passed else 'FAIL'} {name}: x={center}, expected {expected}", flush=True)
    return results


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("engine", type=Path)
    parser.add_argument("--artifacts", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="lwe-particle-starttime-") as directory:
        root = Path(directory) / "case"
        try:
            scene, probes = build_scene(root)
            image, _ = capture(args.engine.resolve(), root, scene)
            results = check_centers(image, probes)
            write_json(root / "checks.json", results)
        finally:
            if args.artifacts:
                shutil.copytree(root, args.artifacts, dirs_exist_ok=True, ignore=shutil.ignore_patterns("*.sock"))
    raise SystemExit(0 if all(check["passed"] for check in results.values()) else 1)
