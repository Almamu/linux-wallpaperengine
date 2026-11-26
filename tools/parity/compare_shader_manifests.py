#!/usr/bin/env python3
"""Compare native DXBC feature evidence with WPE_DUMP_SHADERS units, offline.

Usage: compare_shader_manifests.py NATIVE_ITEM_OR_JSON DUMP_DIR --json report.json
Requires glslangValidator and spirv-dis; no renderer is launched. Each GLSL unit
is compiled to optimized OpenGL SPIR-V. Only surviving uniforms and comparison
instructions count as use; source #defines are recorded separately.

Default output is an inventory, never material/pass equivalence. Optional
--mapping JSON is a list of {"native_blob": "full hash", "fork_units":
["genericimage4.1.vert", "genericimage4.2.frag"], "evidence": "independent trace"}.
Paths are relative to DUMP_DIR. Mapping assertions remain reviewer evidence;
filenames, neighboring dump counters and equal inventories do not establish one.
Exit 0 means analysis completed (differences are reported, not a failure), 1 means
some analysis/mapping is unresolved, and 2 means invalid inputs or missing tools.
"""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile

import native_shader_manifest


FEATURE_UNIFORMS = {
    'fog_distance': ('g_FogDistance',),
    'fog_height': ('g_FogHeight',),
    'light_point': ('g_LPoint_',),
    'light_spot': ('g_LSpot_',),
    'light_directional': ('g_LDirectional_',),
    'projected_shadow': ('g_LFeature_ShadowProjection',),
    'shadow_point': ('g_LFeature_ShadowPointProjection',),
    'foliage': ('g_TreeHeight', 'g_TreeRadius', 'g_Foliage'),
    'skinning': ('g_Bones', 'u_Bones'),
}
LIMITATIONS = [
    'Native cache entries may come from earlier runs/settings; use a fresh private cache for attribution.',
    'Cache hashes and dump filenames do not identify a shared material/pass. Global dump counters do not pair stages.',
    'Inventories count native programs and separate fork units, not equivalent draws or variants.',
    'CModel custom shadowcaster programs bypass WPE_DUMP_SHADERS; absent dumps do not prove absent support.',
    'Volumetric shaders are changed after dumping; these units do not include every runtime transformation.',
    'Offline optimized compiler use is not driver/runtime execution. Separate vertex outputs may be unused by a fragment partner.',
    'Feature names use known uniform conventions; renamed or differently implemented features may not be recognized.',
    'Static comparison instruction counts are not dynamic tap counts and are not compared across compiler backends.',
    'Features do not verify uniform values, light budgets, shadow atlas/bias/casters, render state, or visual equivalence.',
]


def features_from_uniforms(names, comparisons):
    features = {feature for feature, prefixes in FEATURE_UNIFORMS.items()
                if any(name.startswith(prefixes) for name in names)}
    if comparisons:
        features.add('shadow_sampling')
    return sorted(features)


def native_features(entry):
    declared, used = set(), set()
    comparisons = 0
    for stage in entry['stages']:
        for buffer in stage['buffers']:
            for variable in buffer['variables']:
                declared.add(variable['name'])
                if variable['used']:
                    used.add(variable['name'])
        comparisons += sum(count for opcode, count in stage['opcodes'].items()
                           if opcode in ('sample_c', 'sample_c_lz'))
    return {'id': entry['blob'], 'stages': [stage['stage'] for stage in entry['stages']],
            'kind_hint': entry.get('summary', {}).get('kind'),
            'features': features_from_uniforms(used, comparisons),
            'used_uniforms': sorted(used), 'declared_unused_uniforms': sorted(declared - used),
            'static_shadow_compare_instructions': comparisons}


def run_tool(command):
    result = subprocess.run(command, capture_output=True, text=True, timeout=60)
    if result.returncode:
        raise ValueError((result.stdout + result.stderr).strip())
    return result.stdout


def fork_features(path, glslang='glslangValidator', spirv_dis='spirv-dis', *, cache=None):
    result = {'id': str(path), 'stage': {'.vert': 'VS', '.frag': 'PS', '.geom': 'GS'}[path.suffix],
              'status': 'unresolved', 'features': None}
    key = None
    try:
        source = path.read_bytes()
        result['sha256'] = hashlib.sha256(source).hexdigest()
        key = (result['stage'], result['sha256'])
        if cache is not None and key in cache:
            cached = cache[key]
            result = dict(cached, id=str(path))
            if 'error' in result:
                result['error'] = result['error'].replace(str(Path(cached['id']).resolve()), str(path.resolve()))
            return result
        text = source.decode('utf-8')
        result['source_defines'] = re.findall(r'^[ \t]*#[ \t]*define[^\n]*', text, re.MULTILINE)
        with tempfile.TemporaryDirectory(prefix='lwe-shader-manifest-') as directory:
            spirv = str(Path(directory) / 'unit.spv')
            run_tool([glslang, '-G', '--aml', '--amb', '-Os', '-o', spirv, str(path.resolve())])
            assembly = run_tool([spirv_dis, spirv])
        names = dict(re.findall(r'^\s*OpName (%\S+) "([^"]*)"', assembly, re.MULTILINE))
        uniforms = re.findall(r'^\s*(%\S+) = OpVariable \S+ (?:UniformConstant|Uniform)\b',
                              assembly, re.MULTILINE)
        used = sorted({names[variable] for variable in uniforms if variable in names})
        comparisons = len(re.findall(r'^\s*%\S+ = OpImage\w*Dref\w*\b', assembly, re.MULTILINE))
        result.update(status='analyzed', used_uniforms=used,
                      features=features_from_uniforms(used, comparisons),
                      static_shadow_compare_instructions=comparisons)
    except (ValueError, OSError, subprocess.TimeoutExpired) as error:
        result['error'] = str(error)
    if cache is not None and key is not None:
        cache[key] = dict(result)
    return result


def read_native(path):
    if path.is_file():
        entries = json.loads(path.read_text())['programs']
    else:
        directory = path / 'shaders' / 'blobsSM40'
        if not directory.is_dir():
            directory = path
        entries = [native_shader_manifest.describe(native_shader_manifest.read_blob(blob))
                   for blob in sorted(directory.glob('*.dxs'))]
    if not entries or any(not entry.get('stages') for entry in entries):
        raise ValueError(f'no reflected native programs in {path}')
    return entries


def read_fork(directory, glslang='glslangValidator', spirv_dis='spirv-dis'):
    paths = sorted(path for path in directory.rglob('*')
                   if path.is_file() and path.suffix in ('.vert', '.frag', '.geom'))
    if not paths:
        raise ValueError(f'no shader dumps in {directory}')
    units, cache = [], {}
    for path in paths:
        unit = fork_features(path, glslang, spirv_dis, cache=cache)
        unit['id'] = path.relative_to(directory).as_posix()
        units.append(unit)
    return units


def compare_manifests(native_entries, fork_units, mapping=()):
    native = {entry['blob']: native_features(entry) for entry in native_entries}
    if len(native) != len(native_entries):
        raise ValueError('duplicate native blob IDs')
    fork = {unit['id']: unit for unit in fork_units}
    unresolved = sorted(unit['id'] for unit in fork_units if unit['status'] != 'analyzed')
    inventory = {}
    for feature in sorted([*FEATURE_UNIFORMS, 'shadow_sampling']):
        native_ids = sorted(key for key, entry in native.items() if feature in entry['features'])
        fork_ids = sorted(key for key, entry in fork.items() if feature in (entry['features'] or []))
        if native_ids and fork_ids:
            status = 'observed_in_both'
        elif native_ids:
            status = 'unresolved_fork_analysis' if unresolved else 'not_observed_in_fork_dumps'
        elif fork_ids:
            status = 'not_observed_in_native_cache'
        else:
            status = 'unresolved_fork_analysis' if unresolved else 'not_observed'
        inventory[feature] = {'status': status, 'native_programs': native_ids, 'fork_units': fork_ids}

    pairs, mapped = [], set()
    for pair in mapping:
        blob, paths = pair['native_blob'], pair['fork_units']
        if not isinstance(pair.get('evidence'), str) or not pair['evidence'].strip():
            raise ValueError('each mapping needs independent material/pass evidence')
        if blob not in native or not isinstance(paths, list) or not paths or any(path not in fork for path in paths):
            raise ValueError(f'unknown native blob or fork unit in mapping: {blob}')
        if blob in mapped:
            raise ValueError(f'duplicate mapping for native blob: {blob}')
        mapped.add(blob)
        units = [fork[path] for path in paths]
        # Stage substitutions need separate evidence; an omitted stage cannot
        # silently become a verified material/pass comparison.
        required = set(native[blob]['stages'])
        stage_counts = collections.Counter(unit['stage'] for unit in units)
        if any(count > 1 for count in stage_counts.values()):
            raise ValueError(f'multiple units for the same stage in mapping: {blob}')
        incomplete = not required.issubset(stage_counts)
        result = dict(pair, status='unresolved', missing_in_fork=None, extra_in_fork=None)
        if incomplete:
            result['reason'] = 'Mapping omits native stages: ' + ', '.join(sorted(required - stage_counts.keys()))
        elif any(unit['status'] != 'analyzed' for unit in units):
            result['reason'] = 'A mapped fork unit could not be analyzed'
        else:
            native_features_set = set(native[blob]['features'])
            fork_features_set = set().union(*(unit['features'] for unit in units))
            missing = sorted(native_features_set - fork_features_set)
            extra = sorted(fork_features_set - native_features_set)
            result.update(status='feature_difference' if missing or extra else 'same_observed_features',
                          missing_in_fork=missing, extra_in_fork=extra)
        pairs.append(result)
    return {'schema_version': 1, 'status': 'mapped_feature_comparison' if mapping else 'inventory_only',
            'equivalence_verified': False, 'limitations': LIMITATIONS,
            'native_programs': list(native.values()), 'fork_units': fork_units,
            'unresolved_fork_units': unresolved, 'unmapped_native_programs': sorted(set(native) - mapped),
            'inventory': inventory, 'mapped_comparisons': pairs}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('native', type=Path, help='native_shader_manifest JSON, workshop item or blob directory')
    parser.add_argument('dumps', type=Path, help='WPE_DUMP_SHADERS directory')
    parser.add_argument('--mapping', type=Path, help='independently established native-to-fork pass mappings')
    parser.add_argument('--json', type=Path, help='write the full report')
    parser.add_argument('--glslang', default='glslangValidator')
    parser.add_argument('--spirv-dis', default='spirv-dis')
    args = parser.parse_args(argv)
    try:
        for tool in (args.glslang, args.spirv_dis):
            if not shutil.which(tool):
                raise ValueError(f'required offline compiler tool not found: {tool}')
        mapping = json.loads(args.mapping.read_text()) if args.mapping else []
        if not isinstance(mapping, list):
            raise ValueError('mapping must be a JSON array')
        report = compare_manifests(read_native(args.native), read_fork(args.dumps, args.glslang, args.spirv_dis), mapping)
        report['sources'] = {'native': str(args.native.resolve()), 'fork_dumps': str(args.dumps.resolve()),
                             'mapping': str(args.mapping.resolve()) if args.mapping else None}
        report['offline_tools'] = {tool: run_tool([tool, '--version']).strip()
                                   for tool in (args.glslang, args.spirv_dis)}
        if args.json:
            args.json.write_text(json.dumps(report, indent=2) + '\n')
    except (OSError, ValueError, KeyError, TypeError, struct.error, subprocess.TimeoutExpired) as error:
        parser.error(str(error))
    print(f"{len(report['native_programs'])} native programs; {len(report['fork_units'])} fork units; "
          f"{len(report['unresolved_fork_units'])} unresolved units")
    for feature, row in report['inventory'].items():
        print(f"{feature:20} {len(row['native_programs']):3} native / {len(row['fork_units']):3} fork  {row['status']}")
    print(f"{len(report['mapped_comparisons'])} explicit mappings; "
          f"{len(report['unmapped_native_programs'])} unmapped native programs. Equivalence is not verified.")
    for pair in report['mapped_comparisons']:
        if pair['status'] == 'unresolved':
            print(f"{pair['native_blob']}: unresolved: {pair['reason']}")
        else:
            print(f"{pair['native_blob']}: {pair['status']}; missing={','.join(pair['missing_in_fork']) or '-'}; "
                  f"extra={','.join(pair['extra_in_fork']) or '-'}")
    for limitation in LIMITATIONS:
        print(f'Note: {limitation}')
    return 1 if report['unresolved_fork_units'] or any(
        pair['status'] == 'unresolved' for pair in report['mapped_comparisons']) else 0


if __name__ == '__main__':
    sys.exit(main())
