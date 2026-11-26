#!/usr/bin/env python3
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

try:
    import deep_sea_fixtures as fixtures
except ModuleNotFoundError:
    fixtures = None


def shot():
    return {'id': 2238, 'name': 'Heroes 1', 'visible': True,
            'options': {'fps': 30, 'length': 300, 'mode': 'single'},
            'center': {'c0': [
                {'frame': 0, 'value': 3.35324,
                 'front': {'enabled': True, 'x': .50166667, 'y': 0}},
                {'frame': 300, 'value': 2.38255,
                 'back': {'enabled': True, 'x': -.50166667, 'y': 0}}]},
            'eye': {}, 'up': {}, 'fov': None, 'zoom': None}


def scene():
    return {'general': {'bloom': True, 'fogdistance': True, 'camerafade': True,
                        'lightconfig': {'directional': 1, 'point': 3, 'pointshadow': 1, 'spot': 2, 'spotshadow': 2}},
            'camera': {'eye': '0 0 0', 'center': '0 0 -1', 'up': '0 1 0'},
            'objects': [
                {'id': 2233, 'camera': 'default', 'path': 'scripts/camera_paths_2233.json',
                 'visible': {'user': {'name': 'camerastyle', 'condition': '0'}, 'value': True}},
                {'id': 2043, 'camera': 'default', 'path': 'scripts/fixed.json',
                 'visible': {'user': {'name': 'camerastyle', 'condition': '1'}, 'value': False}},
                {'id': 241, 'angles': {'script': 'rotate()', 'value': '0 0 0'}},
                {'id': 122, 'model': 'models/submarine.mdl', 'parent': 241,
                 'animationlayers': [{'rate': 1, 'blend': {'script': 'random()', 'value': 1}}]},
                {'id': 127, 'light': 'lspot', 'castshadow': True, 'parent': 122,
                 'angles': {'animation': {}, 'value': '0 0 -1.7'}},
                {'id': 225, 'light': 'lspot', 'castshadow': True, 'castvolumetrics': True},
                {'id': 399, 'light': 'lpoint', 'castshadow': True},
                {'id': 2087, 'light': 'lpoint', 'intensity': {'animation': {}, 'value': 8.42}},
                {'id': 575, 'image': 'models/shadow_round.json'},
                {'id': 30, 'particle': 'particles/bubbles.json'},
                {'id': 2082, 'image': 'models/mouse.json',
                 'visible': {'script': 'mouse()', 'value': True}},
                {'id': 613, 'visible': False, 'effects': [{'file': 'effects/scroll/effect.json',
                 'passes': [{'constantshadervalues': {'speedx': 1, 'speedy': .32}}]}]}]}


class DeepSeaFixtureTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(fixtures, 'Deep Sea fixture generator must exist')

    def test_cpu_sampler_preserves_authored_bezier_and_fallback_channels(self):
        with tempfile.TemporaryDirectory() as directory:
            samples = fixtures.sample_paths([shot()], [0, 2.5, 5, 10], Path(directory))
        values = samples[0]
        self.assertAlmostEqual(values[0]['center'][0], 3.35324, places=5)
        self.assertAlmostEqual(values[1]['center'][0], 3.17650, delta=.002)
        self.assertAlmostEqual(values[2]['center'][0], 2.867895, delta=.002)
        self.assertAlmostEqual(values[3]['center'][0], 2.38255, delta=.001)
        self.assertEqual(values[1]['up'], [0, 1, 0])
        self.assertEqual(values[1]['zoom'], 1)

    def test_freeze_removes_scripted_animation_but_preserves_parented_geometry(self):
        original = scene()
        frozen, report = fixtures.freeze_scene(original, 'baseline')
        objects = {obj['id']: obj for obj in frozen['objects']}
        self.assertEqual(objects[241]['angles'], '0 0 0')
        self.assertTrue(objects[241].get('visible', True))
        self.assertTrue(objects[122].get('visible', True))
        self.assertTrue(objects[127].get('visible', True))
        self.assertEqual(objects[122]['animationlayers'], [{'rate': 0, 'blend': 1}])
        self.assertEqual(objects[127]['angles'], '0 0 -1.7')
        self.assertEqual(objects[2087]['intensity'], 8.42)
        self.assertFalse(objects[30]['visible'])
        self.assertFalse(objects[2082]['visible'])
        self.assertEqual(objects[2233]['visible']['user']['name'], 'camerastyle')
        self.assertEqual(objects[613]['effects'][0]['passes'][0]['constantshadervalues']['speedy'], 0)
        self.assertFalse(frozen['general']['camerafade'])
        self.assertIsInstance(original['objects'][2]['angles'], dict)
        self.assertFalse(report['pixel_comparable'])
        self.assertTrue(report['limitations'])

    def test_ablations_change_the_requested_rendering_feature(self):
        for variant, expected in [('no-cap', 575), ('no-volumetrics', 225)]:
            frozen, _ = fixtures.freeze_scene(scene(), variant)
            target = next(obj for obj in frozen['objects'] if obj['id'] == expected)
            self.assertFalse(target['visible' if variant == 'no-cap' else 'castvolumetrics'])
        for variant, field in [('no-fog', 'fogdistance'), ('no-bloom', 'bloom')]:
            frozen, _ = fixtures.freeze_scene(scene(), variant)
            self.assertFalse(frozen['general'][field])
        frozen, _ = fixtures.freeze_scene(scene(), 'no-shadows')
        self.assertTrue(all(not obj['castshadow'] for obj in frozen['objects'] if obj['id'] in (127, 225, 399)))
        frozen, _ = fixtures.freeze_scene(scene(), 'shadow-127')
        self.assertFalse(frozen['general']['bloom'])
        self.assertFalse(frozen['general']['fogdistance'])
        lights = {obj['id']: obj.get('visible', True) for obj in frozen['objects'] if 'light' in obj}
        self.assertEqual(lights, {127: True, 225: False, 399: False, 2087: True})

    def test_freezing_scripted_visibility_preserves_authored_user_gate(self):
        original = scene()
        gate = {'name': 'camerastyle', 'condition': '0'}
        original['objects'][0]['visible'].update(script='return value;', scriptproperties={})
        frozen, _ = fixtures.freeze_scene(original, 'baseline')
        self.assertEqual(frozen['objects'][0]['visible'], {'user': gate, 'value': True})
        fixtures.add_ready_marker(frozen, 0, 'fixture-token')
        self.assertEqual(frozen['objects'][0]['visible']['user'], gate)

    def test_no_shadows_reallocates_native_light_budgets_without_dropping_lights(self):
        original = scene()
        frozen, report = fixtures.freeze_scene(original, 'no-shadows')
        self.assertEqual(frozen['general']['lightconfig'],
                         {'directional': 1, 'point': 3, 'pointshadow': 0, 'spot': 2, 'spotshadow': 0})
        self.assertEqual(original['general']['lightconfig']['pointshadow'], 1)
        self.assertEqual(original['general']['lightconfig']['spotshadow'], 2)
        lights = [obj for obj in frozen['objects'] if obj['id'] in (127, 225, 399)]
        self.assertEqual(len(lights), 3)
        self.assertTrue(all(obj.get('visible', True) for obj in lights))
        self.assertTrue(next(obj for obj in lights if obj['id'] == 225)['castvolumetrics'])
        self.assertEqual(report['lightconfig_changes'],
                         {'pointshadow': {'before': 1, 'after': 0}, 'spotshadow': {'before': 2, 'after': 0}})

    def test_foliage_time_controls_are_zero_without_removing_the_shader(self):
        material = {'passes': [{'shader': 'foliage4', 'constantshadervalues': {'tint': '1 1 1'}}]}
        frozen = fixtures.freeze_material(material)
        self.assertEqual(frozen['passes'][0]['shader'], 'foliage4')
        self.assertEqual(frozen['passes'][0]['constantshadervalues'].get('foliagespeedbase'), 0)
        self.assertEqual(frozen['passes'][0]['constantshadervalues'].get('foliagespeedleaves'), 0)
        self.assertNotIn('foliagespeedbase', material['passes'][0]['constantshadervalues'])

    def test_ready_marker_forwards_visibility_and_logs_only_the_first_update(self):
        add_marker = getattr(fixtures, 'add_ready_marker', None)
        self.assertIsNotNone(add_marker, 'Fixtures need a first-update readiness marker')
        for style, camera_id in [(0, 2233), (1, 2043)]:
            frozen, _ = fixtures.freeze_scene(scene(), 'baseline')
            before_ids = [obj['id'] for obj in frozen['objects']]
            add_marker(frozen, style, 'LWE_DEEP_SEA_READY_test')
            self.assertEqual([obj['id'] for obj in frozen['objects']], before_ids)
            camera = next(obj for obj in frozen['objects'] if obj['id'] == camera_id)
            visible = camera['visible']
            self.assertTrue(visible['value'])
            self.assertEqual(visible['user'], {'name': 'camerastyle', 'condition': str(style)})
            script = visible['script'].replace('export function', 'function')
            harness = "const messages=[]; const console={error:value=>messages.push(value)}; "
            harness += "const engine={runtime:1.25}; " + script
            harness += "\nconst values=[update(true),update(false),update(true)]; "
            harness += "process.stdout.write(JSON.stringify({values,messages}));"
            run = subprocess.run(['node', '-e', harness], capture_output=True, text=True, check=True)
            result = json.loads(run.stdout)
            self.assertEqual(result['values'], [True, False, True])
            self.assertEqual(len(result['messages']), 1)
            marker, payload = result['messages'][0].split(' ', 1)
            self.assertEqual(marker, 'LWE_DEEP_SEA_READY_test')
            self.assertEqual(json.loads(payload)['runtime'], 1.25)

    def test_generated_assets_are_private_bounded_and_capture_compatible(self):
        from capture import source_manifest
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'source'
            extracted = root / 'extracted'
            source.mkdir()
            (extracted / 'scripts').mkdir(parents=True)
            (extracted / 'materials').mkdir()
            (extracted / 'shaders/blobsSM40').mkdir(parents=True)
            (extracted / 'shaders/blobsSM40/stale.dxs').write_bytes(b'cache')
            project = {'type': 'scene', 'file': 'scene.json', 'general': {'properties': {
                key: {'value': value} for key, value in [('camerastyle', '0'), ('showclock', True),
                                                       ('waterdistortion', True)]}}}
            (source / 'project.json').write_text(json.dumps(project))
            (extracted / 'scene.json').write_text(json.dumps(scene()))
            (extracted / 'scripts/camera_paths_2233.json').write_text(json.dumps({'paths': [shot()]}))
            # Loose overrides win over package contents in the renderer.
            loose_scene = scene()
            loose_scene['general']['nearz'] = 1.2
            (source / 'scene.json').write_text(json.dumps(loose_scene))
            (source / 'scripts').mkdir()
            loose_shot = shot()
            loose_shot['name'] = 'Loose camera override'
            (source / 'scripts/camera_paths_2233.json').write_text(json.dumps({'paths': [loose_shot]}))
            (extracted / 'materials/asset.tex').write_bytes(b'unchanged texture')
            before = source_manifest(source)
            extracted_before = source_manifest(extracted)
            def sample(paths, times, directory):
                self.assertEqual(paths, [loose_shot])
                self.assertEqual(times, [0, 5])
                directory.mkdir()
                (directory / 'camera_sampler').write_bytes(b'CPU sampler identity fixture')
                return [[dict(center=[3.35, 0, -1], eye=[0, 0, 0], up=[0, 1, 0], fov=50, zoom=1),
                         dict(center=[2.87, 0, -1], eye=[0, 0, 0], up=[0, 1, 0], fov=50, zoom=1)]]

            with patch.object(fixtures, 'sample_paths', side_effect=sample):
                result = fixtures.generate(source, root / 'output', extracted, times=[0, 5],
                                           variants=['baseline', 'no-cap'])
            self.assertEqual(len(result['fixtures']), 4)
            markers = [entry.get('ready_marker') for entry in result['fixtures']]
            self.assertTrue(all(markers))
            self.assertEqual(len(set(markers)), 4)
            self.assertEqual(source_manifest(source), before)
            self.assertEqual(source_manifest(extracted), extracted_before)
            items = [Path(entry['path']) for entry in result['fixtures']]
            for item, center in zip(items, (3.35, 3.35, 2.87, 2.87)):
                self.assertTrue(source_manifest(item))
                self.assertFalse((item / 'shaders/blobsSM40/stale.dxs').exists())
                self.assertFalse((item / 'scene.pkg').exists())
                path = json.loads((item / 'scripts/camera_paths_2233.json').read_text())['paths'][0]
                self.assertEqual(path['options']['mode'], 'loop')
                # Native Submarine 3 control: paused paths preserve the rendered
                # pose while eliminating the loop sampler's float-rounding drift.
                self.assertTrue(path['options'].get('startpaused'))
                self.assertEqual(path['center']['c0'], [{'frame': 0, 'value': center}])
                self.assertEqual(path['name'], 'Loose camera override')
                self.assertEqual(json.loads((item / 'scene.json').read_text())['general'].get('nearz'), 1.2)
                props = json.loads((item / 'project.json').read_text())['general']['properties']
                self.assertFalse(props['showclock']['value'])
                self.assertFalse(props['waterdistortion']['value'])
            self.assertEqual((items[0] / 'materials/asset.tex').stat().st_ino,
                             (items[1] / 'materials/asset.tex').stat().st_ino)
            self.assertNotEqual((items[0] / 'scene.json').stat().st_ino,
                                (items[1] / 'scene.json').stat().st_ino)
            self.assertNotEqual((items[0] / 'materials/asset.tex').stat().st_ino,
                                (extracted / 'materials/asset.tex').stat().st_ino)
            self.assertFalse(result['pixel_comparable'])

    def test_refuses_to_write_inside_source_or_reuse_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with self.assertRaises(ValueError):
                fixtures.generate(root, root / 'nested', root)
            other = root.parent / root.name
            with self.assertRaises((ValueError, FileExistsError)):
                fixtures.generate(root, other, root)


if __name__ == '__main__':
    unittest.main()
