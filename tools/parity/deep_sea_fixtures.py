#!/usr/bin/env python3
"""Build private frozen Deep Sea Dive shots; runtime repeatability remains unverified.

Example: deep_sea_fixtures.py WORKSHOP_ITEM build/parity-deep-sea/fixtures
         --extracted build/parity-deep-sea/extracted --include-fixed
Immutable assets share hardlinks within the private bank, never with the input.
Capture tools must copy an item before rendering it (capture.py prepare does).
"""
import argparse
import copy
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import subprocess

from parity import asset_digest, sha256
from capture import source_manifest

REPO = Path(__file__).resolve().parents[2]
CAMERA_PATH = 'scripts/camera_paths_2233.json'
VARIANTS = ('baseline', 'no-cap', 'no-fog', 'no-bloom', 'no-shadows',
            'shadow-399', 'shadow-127', 'shadow-225', 'no-volumetrics', 'ambient-only')
LIMITATIONS = [
    'Native and fork animation rate=0 initial skeleton pose requires runtime verification.',
    'Embedded Brendan/May spring physics remains active independently of animation rate=0.',
    'Paused camera paths eliminate interpolation-phase drift; whole-scene repeatability remains unverified across all shots.',
    'Shader time controls and hidden dependency rendering require temporal repeat captures in both engines.',
    'GPU quality, color transfer, volumetric jitter and residual runtime behavior are not certified by JSON edits.',
    'Particles, audio, the clock, manual camera input and animated playback are excluded from this fixture.',
]

# Only the JSON-to-channel adapter lives here. All curve solving, integer-frame
# interpolation, normalization and clamping execute the renderer's actual C++.
# Wrapped/legacy paths are explicitly refused instead of approximated.
SAMPLER = r'''
#include "WallpaperEngine/Data/Model/CameraPath.h"
#include <nlohmann/json.hpp>
#include <iostream>
using namespace WallpaperEngine::Data::Model;
using J = nlohmann::json;
CameraPathChannel channel(const J& data) {
    CameraPathChannel result;
    if (!data.is_array()) return result;
    int last = -1;
    for (const auto& source : data) {
        if (!source.is_object()) continue;
        int frame = source.value("frame", 0);
        if (frame <= last) continue;
        last = frame;
        CameraPathKeyframe key;
        key.frame = frame;
        key.value = source.value("value", 0.0f);
        key.step = source.value("step", false);
        auto handle = [&](const char* name, CameraPathHandle& target) {
            if (key.step || !source.contains(name) || !source[name].is_object()) return;
            const auto& h = source[name];
            if (!h.value("enabled", false)) return;
            target.enabled = true;
            target.offset = {h.value("x", 0.0f), h.value("y", 0.0f)};
        };
        handle("back", key.incoming);
        handle("front", key.outgoing);
        result.keyframes.push_back(key);
    }
    return result;
}
int main() {
    J input; std::cin >> input;
    J output = J::array();
    for (const auto& source : input["paths"]) {
        CameraPath path;
        path.lengthFrames = source["options"]["length"];
        path.secondsPerFrame = 1.0f / source["options"]["fps"].get<float>();
        path.fov = channel(source.value("fov", J()));
        path.zoom = channel(source.value("zoom", J()));
        for (int i=0; i<3; ++i) {
            const auto name = "c" + std::to_string(i);
            path.center[i] = channel(source.value("center", J::object()).value(name, J()));
            path.eye[i] = channel(source.value("eye", J::object()).value(name, J()));
            path.up[i] = channel(source.value("up", J::object()).value(name, J()));
        }
        J samples = J::array();
        for (float time : input["times"]) {
            const auto value = path.evaluate(time, CameraTransform{});
            samples.push_back({{"center", {value.center.x, value.center.y, value.center.z}},
                {"eye", {value.eye.x, value.eye.y, value.eye.z}},
                {"up", {value.up.x, value.up.y, value.up.z}},
                {"fov", value.fov}, {"zoom", value.zoom}});
        }
        output.push_back(samples);
    }
    std::cout << output.dump();
}
'''


def write_json(path, value):
    """Replace the inode: never mutate another fixture through a hardlink."""
    temporary = path.with_name(path.name + '.new')
    with temporary.open('x', encoding='utf-8') as output:
        json.dump(value, output, indent=2, allow_nan=False)
        output.write('\n')
    temporary.replace(path)


def sample_paths(paths, times, directory):
    for path in paths:
        options = path.get('options', {})
        if ('transforms' in path or options.get('wraploop') or
                options.get('fps', 0) <= 0 or options.get('length', 0) <= 0):
            raise ValueError('Sampler accepts positive-length, unwrapped authored curve paths only')
        if any(not math.isfinite(t) or t < 0 or t > options['length'] / options['fps'] for t in times):
            raise ValueError('Sample times must lie inside the authored shot duration')
    directory.mkdir(parents=True, exist_ok=True)
    source = directory / 'camera_sampler.cpp'
    binary = directory / 'camera_sampler'
    source.write_text(SAMPLER, encoding='utf-8')
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++20', '-O2',
                    '-I', str(REPO / 'src'), '-I', str(REPO / 'src/External/json/single_include'),
                    str(source), str(REPO / 'src/WallpaperEngine/Data/Model/CameraPath.cpp'),
                    '-o', str(binary)], check=True, capture_output=True, text=True)
    output = subprocess.run([str(binary)], input=json.dumps({'paths': paths, 'times': times}),
                            capture_output=True, text=True, check=True)
    return json.loads(output.stdout)


def freeze_scene(source, variant):
    if variant not in VARIANTS:
        raise ValueError(f'Unsupported variant: {variant}')
    frozen_properties = []
    unresolved = []

    def freeze(value, location):
        if isinstance(value, dict):
            if 'script' in value or 'animation' in value and isinstance(value['animation'], dict):
                if 'value' in value:
                    frozen_properties.append(location)
                    constant = freeze(value['value'], f'{location}.value')
                    if value.get('user') is not None:
                        return {'user': copy.deepcopy(value['user']), 'value': constant}
                    return constant
                if 'script' in value:
                    unresolved.append(f'Script without a constant value: {location}')
            return {key: freeze(child, f'{location}.{key}') for key, child in value.items()}
        if isinstance(value, list):
            return [freeze(child, f'{location}[{index}]') for index, child in enumerate(value)]
        return value

    scene = freeze(source, 'scene')
    general = scene['general']
    general.update(camerafade=False, camerashake=False, cameraparallax=False, windenabled=False)
    lightconfig_changes = {}
    if variant == 'no-shadows':
        # Native partitions each total light budget into shadowed and ordinary
        # slots. Removing castshadow alone leaves ordinary spots with zero slots.
        budgets = general.setdefault('lightconfig', {})
        for name in ('pointshadow', 'spotshadow'):
            lightconfig_changes[name] = {'before': budgets.get(name, 0), 'after': 0}
            budgets[name] = 0
    objects = {obj['id']: obj for obj in scene['objects']}
    for obj in objects.values():
        for layer in obj.get('animationlayers', []):
            layer['rate'] = 0
        if 'particle' in obj or 'sound' in obj or obj['id'] == 2082:
            obj['visible'] = False
        if obj.get('camera'):
            obj['queuemode'] = 'sequential'
    # The hidden image is still rendered for its seaweed texture consumers.
    for effect in objects.get(613, {}).get('effects', []):
        if effect.get('file') == 'effects/scroll/effect.json':
            for render_pass in effect.get('passes', []):
                render_pass.setdefault('constantshadervalues', {}).update(speedx=0, speedy=0)
    if variant == 'no-cap':
        objects[575]['visible'] = False
    if variant == 'no-fog' or variant.startswith('shadow-'):
        general['fogdistance'] = False
    if variant == 'no-bloom' or variant.startswith('shadow-'):
        general['bloom'] = False
    for light_id in (399, 127, 225):
        if variant == 'no-shadows':
            objects[light_id]['castshadow'] = False
        elif variant.startswith('shadow-'):
            objects[light_id]['visible'] = light_id == int(variant.split('-')[1])
    for obj in objects.values():
        if 'light' in obj:
            if variant == 'no-volumetrics':
                obj['castvolumetrics'] = False
            if variant == 'ambient-only':
                obj['visible'] = False
    return scene, {'pixel_comparable': False, 'frozen_properties': frozen_properties,
                   'limitations': LIMITATIONS + unresolved,
                   'lightconfig_changes': lightconfig_changes,
                   'groups': 'Constant transforms retained; hiding groups would remove geometry and shadow lights.',
                   'shadow_variant_scope': 'Single shadow light retains the non-shadowed lights and ambient illumination.'}


def freeze_material(source):
    material = copy.deepcopy(source)
    for render_pass in material.get('passes', []):
        if render_pass.get('shader') == 'foliage4':
            render_pass.setdefault('constantshadervalues', {}).update(foliagespeedleaves=0, foliagespeedbase=0)
    return material


def add_ready_marker(scene, style, marker):
    """Observe the selected camera's first update without adding a draw/object."""
    selected = [obj for obj in scene['objects'] if 'camera' in obj
                and isinstance(obj.get('visible'), dict)
                and obj['visible'].get('user') == {'name': 'camerastyle', 'condition': str(style)}]
    if len(selected) != 1:
        raise ValueError(f'Expected exactly one authored camera for style {style}')
    visible = selected[0]['visible']
    # Fixed cameras author false as their inactive default. This fixture selects
    # this camera, so true is its resolved value; retain the authored user gate.
    visible['value'] = True
    visible['script'] = (
        "'use strict'; let sent=false; export function update(value) { "
        "if (!sent) { sent=true; console.error(" + json.dumps(marker + ' ') +
        "+JSON.stringify({runtime:engine.runtime,at:Date.now()})); } return value; }")


def constant_path(path, pose):
    # Even identical adjacent values can round differently as their blend phase
    # advances. Native/fork Submarine 3 repeats verified a paused path holds the
    # same pose with zero pixel motion; keep its sampled phase at frame zero.
    result = {'id': path['id'], 'name': path['name'], 'visible': True,
              'options': {'fps': path['options']['fps'], 'length': path['options']['length'],
                          'mode': 'loop', 'startpaused': True}}
    for name in ('center', 'eye', 'up'):
        result[name] = {f'c{i}': [{'frame': 0, 'value': value}] for i, value in enumerate(pose[name])}
    for name in ('fov', 'zoom'):
        result[name] = [{'frame': 0, 'value': pose[name]}]
    return {'paths': [result]}


def cache_or_package(path):
    return path.suffix.lower() == '.pkg' or any(part.startswith('blobsSM') for part in path.parts)


def copy_inputs(source, destination):
    for entry in source_manifest(source):
        relative = Path(entry['path'])
        if cache_or_package(relative):
            continue
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source / relative, target)


def generate(source, output, extracted=None, shots=None, times=(0, 5, 10),
             variants=('baseline',), include_fixed=False):
    source, output = Path(source).resolve(), Path(output).resolve()
    extracted = Path(extracted).resolve() if extracted else None
    if output.is_relative_to(source) or extracted and output.is_relative_to(extracted):
        raise ValueError('Output must be outside original and extracted inputs')
    if output.exists():
        raise FileExistsError(f'Refusing to overwrite fixture bank: {output}')
    if not times or len(set(times)) != len(times) or not variants or len(set(variants)) != len(variants):
        raise ValueError('Times and variants must be nonempty and unique')
    if any(variant not in VARIANTS for variant in variants):
        raise ValueError('Unknown ablation variant')
    original = source_manifest(source)
    project = json.loads((source / 'project.json').read_text(encoding='utf-8-sig'))
    if project.get('file') != 'scene.json' or project.get('type') != 'scene':
        raise ValueError('Expected a scene.json scene project')
    output.mkdir(parents=True)
    if extracted is None:
        if (source / 'scene.json').is_file():
            extracted = source
        else:
            extracted = output / 'extracted'
            repkg = shutil.which('repkg') or str(Path.home() / '.local/bin/repkg')
            with (output / 'extract.log').open('w') as log:
                subprocess.run([repkg, 'extract', str(source / 'scene.pkg'), '-o', str(extracted),
                                '--no-tex-convert', '--en'], stdout=log, stderr=subprocess.STDOUT, check=True)
    extracted_manifest = source_manifest(extracted)
    shared = output / 'shared'
    shared.mkdir()
    copy_inputs(extracted, shared)
    # Workshop loose overrides have precedence over package entries.
    copy_inputs(source, shared)
    scene = json.loads((shared / 'scene.json').read_text(encoding='utf-8-sig'))
    authored = json.loads((shared / CAMERA_PATH).read_text(encoding='utf-8-sig'))['paths']
    selected = list(range(len(authored))) if shots is None else list(shots)
    if not selected or len(set(selected)) != len(selected) or any(i < 0 or i >= len(authored) for i in selected):
        raise ValueError('Shot indices must be unique authored zero-based indices')
    samples = sample_paths([authored[i] for i in selected], times, output / 'sampler')
    material_changes = []
    for path in shared.rglob('*.json'):
        if path.parts[len(shared.parts)] != 'materials':
            continue
        material = json.loads(path.read_text(encoding='utf-8-sig'))
        frozen = freeze_material(material)
        if frozen != material:
            write_json(path, frozen)
            material_changes.append(str(path.relative_to(shared)))
    for path in shared.rglob('*'):
        if path.is_file():
            path.chmod(0o444)
    manifest = {'schema': 1, 'wallpaper_id': '3562141459', 'pixel_comparable': False,
                'source_path': str(source), 'source_files': original, 'source_asset_digest': asset_digest(original),
                'extracted_source': str(extracted), 'extracted_asset_digest': asset_digest(extracted_manifest),
                'generator_sha256': sha256(Path(__file__)),
                'evaluator': {'path': 'src/WallpaperEngine/Data/Model/CameraPath.cpp',
                              'sha256': sha256(REPO / 'src/WallpaperEngine/Data/Model/CameraPath.cpp'),
                              'binary_sha256': sha256(output / 'sampler/camera_sampler'),
                              'method': 'Compiled C++ CameraPath::evaluate; float frame baking and authored Bezier handles'},
                'authored_shots': len(authored), 'material_changes': material_changes,
                'limitations': LIMITATIONS, 'storage': 'Private immutable asset hardlinks; independent edited JSON; no symlinks or copied shader caches.',
                'fixtures': []}

    def emit(entry, path_data, style):
        variant = entry['variant']
        item = output / 'items' / entry['id']
        shutil.copytree(shared, item, copy_function=os.link)
        frozen, report = freeze_scene(scene, variant)
        marker_input = json.dumps({'source': manifest['source_asset_digest'],
                                   'extracted': manifest['extracted_asset_digest'], 'fixture': entry}, sort_keys=True)
        entry['ready_marker'] = 'LWE_DEEP_SEA_READY_' + hashlib.sha256(marker_input.encode()).hexdigest()[:24]
        entry['ready_marker_scope'] = 'First selected-camera update; not a GPU presentation fence or stability proof.'
        add_ready_marker(frozen, style, entry['ready_marker'])
        write_json(item / 'scene.json', frozen)
        properties = {'camerastyle': str(style), 'showclock': False, 'waterdistortion': False}
        metadata = copy.deepcopy(project)
        for key, value in properties.items():
            metadata['general']['properties'][key]['value'] = value
        write_json(item / 'project.json', metadata)
        if path_data is not None:
            write_json(item / CAMERA_PATH, path_data)
        entry.update(path=str(item), properties=properties, **report)
        write_json(item / 'fixture-manifest.json', dict(entry, schema=1, material_changes=material_changes,
                                                     source_asset_digest=manifest['source_asset_digest']))
        entry['asset_digest'] = asset_digest(source_manifest(item))
        manifest['fixtures'].append(entry)

    for index, poses in zip(selected, samples):
        path = authored[index]
        for time, pose in zip(times, poses):
            for variant in variants:
                identity = f'shot-{index:02d}-{path["id"]}-t{time:g}-{variant}'
                emit({'id': identity, 'shot_index': index, 'shot_id': path['id'], 'shot_name': path['name'],
                      'time_seconds': time, 'variant': variant, 'pose': pose}, constant_path(path, pose), 0)
    if include_fixed:
        for style in range(1, 18):
            for variant in variants:
                emit({'id': f'fixed-{style:02d}-{variant}', 'shot_index': None, 'shot_id': None,
                      'shot_name': f'Fixed style {style}', 'time_seconds': 0, 'variant': variant}, None, style)
    if source_manifest(source) != original or source_manifest(extracted) != extracted_manifest:
        raise ValueError('Original inputs changed during generation; fixture bank is unverified')
    manifest['source_unchanged'] = True
    write_json(output / 'manifest.json', manifest)
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path, help='new private fixture bank directory on disk')
    parser.add_argument('--extracted', type=Path, help='existing unmodified repkg --no-tex-convert output')
    parser.add_argument('--shots', help='comma-separated zero-based authored indices; default all 49')
    parser.add_argument('--times', default='0,5,10')
    parser.add_argument('--variants', default='baseline', help=','.join(VARIANTS))
    parser.add_argument('--include-fixed', action='store_true', help='also generate styles 1 through 17')
    args = parser.parse_args()
    result = generate(args.source, args.output, args.extracted,
                      [int(value) for value in args.shots.split(',')] if args.shots else None,
                      [float(value) for value in args.times.split(',')], args.variants.split(','), args.include_fixed)
    print(json.dumps({'manifest': str(args.output.resolve() / 'manifest.json'),
                      'fixtures': len(result['fixtures']), 'source_unchanged': result['source_unchanged'],
                      'pixel_comparable': result['pixel_comparable']}))


if __name__ == '__main__':
    main()
