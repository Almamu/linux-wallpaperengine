#!/usr/bin/env python3
"""CPU-only reflection tests; fixtures use the stock/fork uniform vocabulary."""
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

import compare_shader_manifests as compare
import native_shader_manifest as native


def native_program(variables, *, blob='native-image', taps=0):
    # Minimal native_shader_manifest.read_blob output, including declaration-only
    # flags found in Deep Sea Dive's unfogged genericimage4 programs.
    return {'blob': blob, 'bytes': 100, 'container': 'SHDV', 'stages': [{
        'stage': 'PS', 'instructions': 8, 'inputs': [], 'outputs': [],
        'buffers': [{'name': 'Constants', 'bytes': 128, 'variables': [
            {'name': name, 'bytes': 16, 'used': used} for name, used in variables]}],
        'bindings': [], 'opcodes': {'sample_c': taps} if taps else {},
    }]}


class NativeFeatureTests(unittest.TestCase):
    def test_sm4_blob_decodes_two_variables_and_ignores_magic_inside_reflection(self):
        # SM4 RDEF has six DWORDs per variable. The second constant is live.
        names = b'Constants\0g_BonesDXBC\0g_FogDistanceParams\0'
        rdef = struct.pack('<7I', 1, 28, 0, 0, 0xffff0400, 0, 0)
        rdef += struct.pack('<6I', 100, 2, 52, 32, 0, 0)
        rdef += struct.pack('<6I', 110, 0, 16, 0, 0, 0)
        rdef += struct.pack('<6I', 122, 16, 16, 2, 0, 0) + names
        shader = struct.pack('<3I', 0x40, 3, (1 << 24) | 70)
        reflection = b'RDEF' + struct.pack('<I', len(rdef)) + rdef
        code = b'SHDR' + struct.pack('<I', len(shader)) + shader
        container = b'DXBC' + bytes(16) + struct.pack('<5I', 1, 40 + len(reflection) + len(code),
                                                      2, 40, 40 + len(reflection)) + reflection + code
        with tempfile.TemporaryDirectory() as directory:
            blob = Path(directory) / 'synthetic.dxs'
            blob.write_bytes(b'SHDV' + container)
            program = native.describe(native.read_blob(blob))
            features = compare.native_features(program)
            self.assertEqual(features['stages'], ['PS'])
            self.assertEqual(features['features'], ['fog_distance', 'shadow_sampling'])
            self.assertEqual(features['declared_unused_uniforms'], ['g_BonesDXBC'])
            self.assertNotIn('skinned', program['summary']['features'])
            blob.write_bytes(b'SHDV' + container[:-1])
            with self.assertRaisesRegex(ValueError, 'DXBC'):
                native.read_blob(blob)

    def test_duplicate_native_ids_are_invalid_input(self):
        with self.assertRaisesRegex(ValueError, 'duplicate native'):
            compare.compare_manifests([native_program([]), native_program([('g_Bones', True)])], [])

    def test_declared_bones_fog_and_foliage_are_not_used_features(self):
        entry = native_program([('g_Bones', False), ('g_FogDistanceParams', False),
                                ('g_TreeHeight', False), ('g_LPoint_Origin', True)])
        result = compare.native_features(entry)
        self.assertEqual(result['features'], ['light_point'])
        self.assertEqual(result['declared_unused_uniforms'],
                         ['g_Bones', 'g_FogDistanceParams', 'g_TreeHeight'])

    def test_compiled_shadow_instructions_and_live_constants_count(self):
        entry = native_program([('g_FogHeightParams', True), ('g_Bones', True),
                                ('g_TreeRadius', True), ('g_LSpot_Origin', True),
                                ('g_LDirectional_Direction', True),
                                ('g_LFeature_ShadowProjectionTransform', True),
                                ('g_LFeature_ShadowPointProjection', True)], taps=27)
        result = compare.native_features(entry)
        self.assertEqual(set(result['features']), {'fog_height', 'skinning', 'foliage',
            'light_spot', 'light_directional', 'projected_shadow', 'shadow_point', 'shadow_sampling'})
        self.assertEqual(result['static_shadow_compare_instructions'], 27)


@unittest.skipUnless(shutil.which('glslangValidator') and shutil.which('spirv-dis'),
                     'glslangValidator and spirv-dis required for offline shader analysis')
class ShaderManifestTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def shader(self, name, body):
        path = self.root / name
        path.write_text('#version 330\n// Processed shader genericimage4\n' + body)
        return path

    def test_defines_unused_helpers_and_dead_code_are_not_feature_usage(self):
        path = self.shader('genericimage4.9.frag', '''
#define FOG_DIST 1
#define FOG_COMPUTED 0
uniform vec4 g_FogDistanceParams;
uniform float g_TreeHeight;
uniform mat4 g_Bones[2];
uniform vec4 g_LPoint_Origin[1];
out vec4 out_FragColor;
float unusedFog() { return g_FogDistanceParams.x; }
void main() {
    float deadAssignment = g_TreeHeight;
    if (false) out_FragColor = g_Bones[0][0];
#if FOG_COMPUTED && FOG_DIST
    out_FragColor = g_FogDistanceParams;
#else
    out_FragColor = g_LPoint_Origin[0];
#endif
}
''')
        result = compare.fork_features(path)
        self.assertEqual(result['status'], 'analyzed')
        self.assertEqual(result['features'], ['light_point'])
        self.assertEqual(result['used_uniforms'], ['g_LPoint_Origin'])
        self.assertIn('#define FOG_DIST 1', result['source_defines'])

    def test_output_contributing_fog_and_comparison_sample_survive(self):
        path = self.shader('genericimage4.41.frag', '''
uniform vec4 g_FogDistanceParams;
uniform sampler2DShadow g_Texture6;
in vec3 v_ViewDir;
out vec4 out_FragColor;
vec4 fog() { return g_FogDistanceParams * length(v_ViewDir); }
void main() { out_FragColor = fog() * texture(g_Texture6, v_ViewDir); }
''')
        result = compare.fork_features(path)
        self.assertEqual(set(result['features']), {'fog_distance', 'shadow_sampling'})
        self.assertEqual(result['static_shadow_compare_instructions'], 1)

    def test_spirv_symbol_name_is_not_a_shadow_comparison_instruction(self):
        path = self.shader('symbol.frag', '''
uniform float OpImageSampleDrefImplicitLod;
out vec4 color;
void main() { color = vec4(OpImageSampleDrefImplicitLod); }
''')
        result = compare.fork_features(path)
        self.assertEqual(result['status'], 'analyzed')
        self.assertEqual(result['static_shadow_compare_instructions'], 0)
        self.assertEqual(result['features'], [])

    def test_vertex_skinning_and_foliage_and_custom_uniform_alias(self):
        path = self.shader('generic3d4.100.vert', '''
uniform mat4 u_Bones[2];
uniform float g_TreeHeight;
in vec4 a_Position;
void main() { gl_Position = u_Bones[1] * (a_Position * g_TreeHeight); }
''')
        self.assertEqual(set(compare.fork_features(path)['features']), {'skinning', 'foliage'})

    def test_compile_failure_is_unknown_not_feature_absence(self):
        path = self.shader('broken.8.frag', 'void main() { broken syntax }')
        result = compare.fork_features(path)
        self.assertEqual(result['status'], 'unresolved')
        self.assertIsNone(result['features'])
        self.assertIn('ERROR', result['error'])
        report = compare.compare_manifests(
            [native_program([('g_FogDistanceParams', True)])], [result])
        self.assertEqual(report['inventory']['fog_distance']['status'], 'unresolved_fork_analysis')

    def test_unreadable_shader_text_is_unresolved_and_other_units_survive(self):
        (self.root / 'bad.1.frag').write_bytes(b'\xff\xfe')
        self.shader('good.2.frag', 'out vec4 c; void main() { c = vec4(1); }')
        units = compare.read_fork(self.root)
        self.assertEqual([unit['status'] for unit in units], ['unresolved', 'analyzed'])

    def test_identical_stage_content_compiles_once_per_read_and_preserves_ids(self):
        body = 'uniform vec4 g_FogDistanceParams; out vec4 c; void main() { c = g_FogDistanceParams; }'
        self.shader('image.1.frag', body)
        self.shader('image.2.frag', body)
        with mock.patch.object(compare, 'run_tool', wraps=compare.run_tool) as calls:
            units = compare.read_fork(self.root)
            self.assertEqual(sum('-G' in call.args[0] for call in calls.call_args_list), 1)
            compare.read_fork(self.root)
            self.assertEqual(sum('-G' in call.args[0] for call in calls.call_args_list), 2)
        self.assertEqual([unit['id'] for unit in units], ['image.1.frag', 'image.2.frag'])
        self.assertEqual([unit['features'] for unit in units], [['fog_distance'], ['fog_distance']])

    def test_distinct_stage_or_content_each_requires_compilation(self):
        for name, second_suffix, second_body in [('stage', 'vert', 'void main() {}'),
                                                ('content', 'frag', 'void main() { discard; }')]:
            with self.subTest(name=name):
                directory = self.root / name
                directory.mkdir()
                (directory / 'a.frag').write_text('#version 330\nvoid main() {}')
                (directory / f'b.{second_suffix}').write_text('#version 330\n' + second_body)
                with mock.patch.object(compare, 'run_tool', wraps=compare.run_tool) as calls:
                    units = compare.read_fork(directory)
                self.assertEqual(sum('-G' in call.args[0] for call in calls.call_args_list), 2)
                self.assertTrue(all(unit['status'] == 'analyzed' for unit in units))

    def test_duplicate_compiler_failure_keeps_each_unit_and_error(self):
        self.shader('broken.1.frag', 'void main() { broken syntax }')
        self.shader('broken.2.frag', 'void main() { broken syntax }')
        with mock.patch.object(compare, 'run_tool', wraps=compare.run_tool) as calls:
            units = compare.read_fork(self.root)
        self.assertEqual(sum('-G' in call.args[0] for call in calls.call_args_list), 1)
        self.assertEqual([unit['id'] for unit in units], ['broken.1.frag', 'broken.2.frag'])
        for unit in units:
            self.assertEqual(unit['status'], 'unresolved')
            self.assertIn('ERROR', unit['error'])

    def test_read_failure_is_not_replaced_by_a_cached_duplicate(self):
        body = 'out vec4 c; void main() { c = vec4(1); }'
        self.shader('a.frag', body)
        broken = self.shader('b.frag', body)
        read_bytes = Path.read_bytes

        def read(path):
            if path == broken:
                raise PermissionError('fixture read denied')
            return read_bytes(path)

        with mock.patch.object(Path, 'read_bytes', read):
            units = compare.read_fork(self.root)
        self.assertEqual([unit['status'] for unit in units], ['analyzed', 'unresolved'])
        self.assertEqual(units[1]['id'], 'b.frag')
        self.assertIn('fixture read denied', units[1]['error'])

    def test_matching_inventory_never_infers_material_or_program_pairing(self):
        path = self.shader('genericimage4.25.frag', '''
uniform vec4 g_FogDistanceParams;
out vec4 out_FragColor;
void main() { out_FragColor = g_FogDistanceParams; }
''')
        report = compare.compare_manifests(
            [native_program([('g_FogDistanceParams', True)])], [compare.fork_features(path)])
        self.assertEqual(report['status'], 'inventory_only')
        self.assertEqual(report['inventory']['fog_distance']['status'], 'observed_in_both')
        self.assertFalse(report['equivalence_verified'])
        self.assertEqual(report['unmapped_native_programs'], ['native-image'])
        self.assertEqual(report['mapped_comparisons'], [])

    def test_explicit_mapping_reports_feature_difference_and_preserves_provenance(self):
        path = self.shader('genericimage4.2.frag', '''
uniform vec4 g_LPoint_Origin[1];
out vec4 out_FragColor;
void main() { out_FragColor = g_LPoint_Origin[0]; }
''')
        units = [compare.fork_features(path)]
        mapping = [{'native_blob': 'native-image', 'fork_units': [str(path)],
                    'evidence': 'Controlled private fixture: one image pass'}]
        report = compare.compare_manifests(
            [native_program([('g_FogDistanceParams', True)])], units, mapping)
        pair = report['mapped_comparisons'][0]
        self.assertEqual(pair['missing_in_fork'], ['fog_distance'])
        self.assertEqual(pair['extra_in_fork'], ['light_point'])
        self.assertEqual(pair['evidence'], mapping[0]['evidence'])
        self.assertFalse(report['equivalence_verified'])

    def test_mapping_without_evidence_or_unknown_unit_is_rejected(self):
        native = [native_program([])]
        for mapping in [
            {'native_blob': 'native-image', 'fork_units': ['missing.frag'], 'evidence': ''},
            {'native_blob': 'native-image', 'fork_units': ['missing.frag'], 'evidence': 'trace'},
        ]:
            with self.subTest(mapping=mapping), self.assertRaises(ValueError):
                compare.compare_manifests(native, [], [mapping])

    def test_mapping_cannot_silently_omit_vertex_skinning(self):
        native = native_program([('g_Bones', True)])
        native['stages'][0]['stage'] = 'VS'
        native['stages'].append(native_program([])['stages'][0])
        path = self.shader('generic3d4.3.frag',
                           'out vec4 color; void main() { color = vec4(1); }')
        mapping = [{'native_blob': 'native-image', 'fork_units': [str(path)], 'evidence': 'pass trace'}]
        report = compare.compare_manifests([native], [compare.fork_features(path)], mapping)
        pair = report['mapped_comparisons'][0]
        self.assertEqual(pair['status'], 'unresolved')
        self.assertIsNone(pair['missing_in_fork'])

    def test_cli_prints_mapped_feature_differences(self):
        native = self.root / 'native.json'
        native.write_text(json.dumps({'programs': [native_program([('g_FogDistanceParams', True)])]}))
        path = self.shader('genericimage4.7.frag',
                           'out vec4 color; void main() { color = vec4(1); }')
        mapping = self.root / 'mapping.json'
        mapping.write_text(json.dumps([{'native_blob': 'native-image', 'fork_units': [path.name],
                                        'evidence': 'single-pass fixture'}]))
        result = subprocess.run([sys.executable, str(Path(compare.__file__)),
            str(native), str(self.root), '--mapping', str(mapping)], text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('missing=fog_distance', result.stdout)

    def test_omitted_native_geometry_stage_remains_unresolved_even_with_equal_features(self):
        native = native_program([('g_Bones', True)])
        native['stages'].append(dict(native['stages'][0], stage='GS'))
        path = self.shader('generic3d4.5.frag', '''
uniform mat4 g_Bones[2];
out vec4 color;
void main() { color = g_Bones[0][0]; }
''')
        mapping = [{'native_blob': 'native-image', 'fork_units': [str(path)], 'evidence': 'trace'}]
        result = compare.compare_manifests([native], [compare.fork_features(path)], mapping)
        self.assertEqual(result['mapped_comparisons'][0]['status'], 'unresolved')

    def test_corrupt_native_blob_is_a_cli_error_without_a_traceback(self):
        (self.root / 'broken.dxs').write_bytes(b'DXBChi')
        result = subprocess.run([sys.executable, str(Path(compare.__file__)),
            str(self.root), str(self.root)], text=True, capture_output=True)
        self.assertEqual(result.returncode, 2)
        self.assertNotIn('Traceback', result.stderr)

    def test_cli_reads_native_manifest_and_reports_unmapped_inventory(self):
        native = self.root / 'native.json'
        native.write_text(json.dumps({'source': 'fixture', 'programs': [native_program([])]}))
        self.shader('genericimage4.1.frag',
                    'out vec4 color; void main() { color = vec4(1); }')
        output = self.root / 'report.json'
        result = subprocess.run([sys.executable, str(Path(compare.__file__)),
            str(native), str(self.root), '--json', str(output)], text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(output.read_text())
        self.assertEqual(report['status'], 'inventory_only')
        self.assertEqual(len(report['fork_units']), 1)
        self.assertTrue(report['limitations'])

    def test_empty_dump_directory_is_not_a_missing_feature_result(self):
        with self.assertRaises(ValueError):
            compare.read_fork(self.root)


if __name__ == '__main__':
    unittest.main()
