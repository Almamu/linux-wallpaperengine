#!/usr/bin/env python3
import json
from pathlib import Path
import tempfile
import unittest
import subprocess
import numpy as np
import determinism


class DeterminismTests(unittest.TestCase):
    def proven(self):
        manifest = determinism.template({'id': '1', 'project_sha256': 'abc'})
        manifest['dependency_coverage'] = {'verified': True, 'evidence': 'All fixture and stock includes traced'}
        manifest['cross_engine_known_inputs'] = {'verified': True, 'evidence': 'Independent uniform trace at fixed simulation states'}
        manifest['observability'] = {'verified': True, 'evidence': 'Authored sentinel proves shader execution', 'probes': {}}
        for source in manifest['sources'].values():
            source.update(state='absent', native={'evidence': 'Reviewed fixture has no such source'},
                          fork={'evidence': 'Reviewed fixture has no such source'})
        return manifest

    def test_unknown_source_is_blocked_at_start(self):
        report = determinism.preflight(determinism.template({'id': '1'}))
        self.assertEqual(report['status'], 'blocked')
        self.assertTrue(any('shader_random' in value for value in report['blockers']))
        self.assertTrue(any('camera_sequence' in value for value in report['blockers']))

    def test_equal_seeds_with_different_rng_algorithms_do_not_match(self):
        controls = self.proven()
        controls['sources']['particle_rng'] = {'state': 'matched',
            'native': {'evidence': 'trace', 'seed': 1, 'algorithm': 'unknown'},
            'fork': {'evidence': 'trace', 'seed': 1, 'algorithm': 'mt19937'}}
        result = determinism.preflight(controls)
        self.assertEqual(result['status'], 'blocked')
        self.assertTrue(any('algorithm' in value for value in result['blockers']))

    def test_camera_shots_are_not_reordered_to_match(self):
        controls = self.proven()
        controls['sources']['camera_sequence'] = {'state': 'matched',
            'native': {'evidence': 'shot trace', 'contract': 'ordered', 'ordered_sequence': ['A', 'B', 'C']},
            'fork': {'evidence': 'shot trace', 'contract': 'ordered', 'ordered_sequence': ['A', 'C', 'B']}}
        self.assertEqual(determinism.preflight(controls)['status'], 'blocked')

    def test_fixed_input_controls_still_need_runtime_repeats(self):
        controls = self.proven()
        controls['sources']['shader_clock'] = {'state': 'fixed',
            'native': {'evidence': 'Input substitution runtime checked', 'value': 1.25},
            'fork': {'evidence': 'Input substitution runtime checked', 'value': 1.25}}
        controls.update(scope='altered_controls', coverage_exclusions=['Original animated shader clock progression'])
        result = determinism.preflight(controls)
        self.assertEqual(result['status'], 'controls_recorded_pending_repeats')
        self.assertFalse(result['determinism_verified'])

    def test_private_input_substitution_is_hash_checked_and_does_not_edit_source(self):
        with tempfile.TemporaryDirectory(dir='/tmp') as directory:
            root = Path(directory)
            (root / 'item/shaders').mkdir(parents=True)
            path = root / 'item/shaders/noise.frag'
            path.write_text('float t = g_Time;\n')
            controls = self.proven()
            controls['substitutions'] = [{'path': 'shaders/noise.frag', 'sha256': 'wrong',
                'old': 'g_Time', 'new': '1.25', 'count': 1, 'input_binding': 'shader clock'}]
            with self.assertRaises(ValueError):
                determinism.apply_substitutions(root, controls)
            self.assertEqual(path.read_text(), 'float t = g_Time;\n')
            controls['substitutions'][0]['sha256'] = determinism.sha256(path)
            changes = determinism.apply_substitutions(root, controls)
            self.assertEqual(path.read_text(), 'float t = 1.25;\n')
            self.assertEqual(changes[0]['input_binding'], 'shader clock')
            self.assertNotEqual(changes[0]['before_sha256'], changes[0]['after_sha256'])

    def test_private_substitution_cannot_escape_item_or_partially_apply(self):
        with tempfile.TemporaryDirectory(dir='/tmp') as directory:
            root = Path(directory)
            (root / 'item').mkdir()
            path = root / 'item/a.frag'
            path.write_text('g_Time')
            controls = self.proven()
            controls['substitutions'] = [
                {'path': 'a.frag', 'sha256': determinism.sha256(path), 'old': 'g_Time', 'new': '1.0',
                 'count': 1, 'input_binding': 'clock'},
                {'path': '../outside.frag', 'sha256': 'bad', 'old': 'x', 'new': 'y',
                 'count': 1, 'input_binding': 'clock'}]
            with self.assertRaises(ValueError):
                determinism.apply_substitutions(root, controls)
            self.assertEqual(path.read_text(), 'g_Time')

    def test_verified_repeats_require_bound_runs_and_observable_nonblack_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            controls = self.proven()
            controls['observability']['probes'] = {
                role: [{'region': [2, 2, 4, 4], 'rgb': [96, 96, 96], 'tolerance': 0}]
                for role in ('reference', 'candidate')}
            for run in ('first', 'second'):
                folder = root / run
                folder.mkdir()
                from parity import asset_digest
                assets = [{'path': 'project.json', 'bytes': 3, 'sha256': 'fixture'}]
                determinism.write_json(folder / 'scenario.json', {'item': controls['item'],
                    'captured_files': assets, 'config_sha256': 'configuration'})
                determinism.write_json(folder / 'binaries.json', {'native': 'native', 'candidate': 'fork'})
                controls['scenario_sha256'] = determinism.sha256(folder / 'scenario.json')
                for role in ('reference', 'candidate'):
                    video = folder / f'{role}.mkv'
                    subprocess.run(['ffmpeg', '-v', 'error', '-f', 'rawvideo', '-pix_fmt', 'rgb24',
                        '-s', '10x8', '-framerate', '10', '-i', '-', '-c:v', 'ffv1', '-pix_fmt', 'bgr0', str(video)],
                        input=np.full((4, 8, 10, 3), 96, np.uint8).tobytes(), check=True)
                    determinism.write_json(folder / f'{role}.json', {'status': 'captured',
                        'video': {'sha256': determinism.sha256(video)}, 'mapped_artifacts': [{'path': role, 'sha256': role}],
                        'mapped_artifacts_after_capture': [{'path': role, 'sha256': role}],
                        'launch_monotonic_ns': 1 if run == 'first' else 2,
                        'acquisition_launch_monotonic_ns': 3 if run == 'first' else 4,
                        'scenario_sha256': controls['scenario_sha256'], 'config_sha256': 'configuration',
                        'input_asset_digest': asset_digest(assets), 'inputs_unchanged_after_capture': True})
            result = determinism.verify_repeats(controls, root / 'first', root / 'second')
            self.assertEqual(result['status'], 'repeatable_for_tested_scenario_window')
            self.assertTrue(result['determinism_verified'])
            changed_driver = root / 'second/reference.json'
            original_driver = changed_driver.read_text()
            metadata = json.loads(original_driver)
            metadata['mapped_artifacts_after_capture'] = [{'path': 'driver', 'sha256': 'changed'}]
            determinism.write_json(changed_driver, metadata)
            self.assertEqual(determinism.verify_repeats(controls, root / 'first', root / 'second')['status'], 'blocked')
            metadata.pop('mapped_artifacts_after_capture')
            determinism.write_json(changed_driver, metadata)
            self.assertEqual(determinism.verify_repeats(controls, root / 'first', root / 'second')['status'], 'blocked')
            changed_driver.write_text(original_driver)
            reused = determinism.verify_repeats(controls, root / 'first', root / 'first')
            self.assertEqual(reused['status'], 'blocked')
            first_meta = root / 'first/reference.json'
            second_meta = root / 'second/reference.json'
            old_second = second_meta.read_text()
            second_meta.write_bytes(first_meta.read_bytes())
            copied = determinism.verify_repeats(controls, root / 'first', root / 'second')
            self.assertEqual(copied['status'], 'blocked')
            second_meta.write_text(old_second)
            for tolerance in (float('nan'), float('inf'), -1, 255):
                with self.assertRaises(ValueError):
                    determinism.check_observability(root / 'first/reference.mkv', [
                        {'region': [1, 1, 2, 2], 'rgb': [128, 64, 32], 'tolerance': tolerance}])
            controls['observability']['probes']['reference'][0]['rgb'] = [0, 0, 0]
            rejected = determinism.verify_repeats(controls, root / 'first', root / 'second')
            self.assertEqual(rejected['status'], 'blocked')
            self.assertTrue(any('observability' in value for value in rejected['blockers']))


if __name__ == '__main__':
    unittest.main()
