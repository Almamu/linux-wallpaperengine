#!/usr/bin/env python3
"""CPU-only sweep checks; fake only application preparation and GPU capture."""
import argparse
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import numpy as np
from PIL import Image

import capture
from parity import asset_digest, sha256, write_json


class SweepTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(importlib.util.find_spec('deep_sea_sweep'),
                             'The batch runner is not implemented')
        import deep_sea_sweep
        self.sweep = deep_sea_sweep
        processes = patch.object(self.sweep, 'private_processes', return_value=[])
        processes.start()
        self.addCleanup(processes.stop)
        self.temporary = tempfile.TemporaryDirectory(prefix='deep-sea-sweep-test-')
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name)
        self.fixtures = []
        for name in ('first', 'broken', 'last'):
            item = self.base / 'fixtures' / name
            item.mkdir(parents=True)
            write_json(item / 'project.json', dict(type='scene', title=name,
                general=dict(properties={'camerastyle': {'value': '0'}})))
            write_json(item / 'scene.json', {'name': name})
            self.fixtures.append(dict(id=name, path=str(item), properties={'camerastyle': '0'},
                shot_name=name, time_seconds=5, variant='baseline', pixel_comparable=False,
                limitations=['Native zero-rate behavior unverified'],
                asset_digest=asset_digest(capture.source_manifest(item))))
        self.manifest = self.base / 'fixtures/manifest.json'
        write_json(self.manifest, dict(schema=1, fixtures=self.fixtures))
        self.binary = self.base / 'renderer'
        self.binary.write_bytes(b'candidate-v1')
        self.adapter = self.base / 'adapter.py'
        self.adapter.write_text('# stub adapter')
        self.proton = self.base / 'proton'
        self.proton.write_text('proton-v1')
        for relative in ('dxvk/x86_64-windows', 'x86_64-windows'):
            runtime = self.base / 'files/lib/wine' / relative
            runtime.mkdir(parents=True)
            for name in ('d3d11.dll', 'dxgi.dll', 'wined3d.dll'):
                (runtime / name).write_bytes(name.encode())
        self.runtime = self.base / 'renderer.so'
        self.runtime.write_bytes(b'library-v1')
        self.prepared = 0
        self.captured = []

    def args(self, **overrides):
        values = dict(manifest=self.manifest, root=self.base / 'private', out=self.base / 'results',
            binary=self.binary, adapter=self.adapter, width=64, height=64, fps=10,
            duration=.2, warmup=0, startup_timeout=1, post_processing='enabled',
            fixture=[], resume=False, native_renderer='dxvk')
        values.update(overrides)
        return argparse.Namespace(**values)

    def prepare(self, args):
        # Retain prepare's filesystem and provenance effects, omit app/prefix launch/copy.
        self.prepared += 1
        args.root.mkdir()
        (args.root / 'app/assets').mkdir(parents=True)
        (args.root / 'app/assets/base').write_bytes(b'native assets')
        (args.root / 'app/wallpaper64.exe').write_bytes(b'native binary')
        write_json(args.root / 'app/config.json', {'steamuser': {'general': {'user': {
            'volumetrics': 'ultra', 'shadows': 'medium'}}}})
        import shutil
        shutil.copytree(args.item, args.root / 'item')
        source = capture.source_manifest(args.item)
        scenario = dict(schema=1, root=str(args.root), width=args.width, height=args.height,
            fps=args.fps, post_processing=args.post_processing, msaa='off', native_renderer=args.native_renderer,
            native_volumetrics='ultra',
            adapter=dict(path=str(args.adapter), sha256=sha256(args.adapter)),
            config_sha256=sha256(args.root / 'app/config.json'),
            item=dict(id=args.item.name, source_path=str(args.item),
                      project_sha256=sha256(args.item / 'project.json'), asset_digest=asset_digest(source)),
            source_files=source, captured_files=capture.source_manifest(args.root / 'item'))
        write_json(args.root / 'scenario.json', scenario)
        return scenario

    def pair(self, args, scenario):
        name = scenario['item']['id']
        self.captured.append(name)
        self.assertEqual(json.loads((args.root / 'item/scene.json').read_text())['name'], name)
        self.assertEqual(capture.check_input_boundary(args.root / 'item', scenario['captured_files']), [])
        self.assertTrue(args.diagnostic)
        args.out.mkdir(parents=True)
        write_json(args.out / 'scenario.json', scenario)
        if name == 'broken':
            raise RuntimeError('synthetic native failure')
        for role, level in (('reference', 20), ('candidate', 21)):
            Image.new('RGB', (64, 64), (level, level, level)).save(args.out / f'worst-{role}.png')
            video = args.out / f'{role}.mkv'
            video.write_bytes(role.encode())
            artifacts = [dict(path=str(self.runtime), sha256=sha256(self.runtime))]
            write_json(args.out / f'{role}.json', dict(status='captured', mapped_artifacts=artifacts,
                mapped_artifacts_after_capture=artifacts, video=dict(path=str(video), sha256=sha256(video))))
        report = dict(status='exploratory', blockers=['Unverified scene time'], metrics={'mae_rgb8': 1.0},
                      pixel_tolerance_rgb8=2)
        write_json(args.out / 'comparison.json', report)
        return report

    def run_fake(self, **overrides):
        with patch.object(capture, 'prepare', side_effect=self.prepare), \
             patch.object(capture, 'adapter_at', return_value=argparse.Namespace(PROTON=self.base)), \
             patch.object(capture, 'run_pair', side_effect=self.pair):
            return self.sweep.run_sweep(self.args(**overrides))

    def test_every_requested_fixture_survives_failure_and_uses_one_app_copy(self):
        report = self.run_fake()
        self.assertEqual(self.prepared, 1)
        self.assertEqual(self.captured, ['first', 'broken', 'last'])
        self.assertEqual([r['status'] for r in report['results']], ['captured', 'capture_failed', 'captured'])
        self.assertIn('synthetic native failure', report['results'][1]['error'])
        self.assertEqual(report['counts'], {'captured': 2, 'capture_failed': 1})
        self.assertFalse(report['pixel_comparable'])
        self.assertEqual(report['results'][0]['still_metrics']['mae_rgb8'], 1)
        self.assertTrue((self.base / 'results/report.html').exists())
        self.assertTrue((self.base / 'results/contact-sheet-001.png').exists())
        self.assertEqual(report['results'][0]['comparison_status'], 'exploratory')

    def test_cli_defaults_to_dxvk_and_explicit_renderer_reaches_preparation(self):
        for renderer in ('dxvk', 'wined3d'):
            with self.subTest(renderer=renderer):
                root, out = self.base / f'private-{renderer}', self.base / f'results-{renderer}'
                argv = ['deep_sea_sweep.py', str(self.manifest), '--root', str(root),
                        '--out', str(out), '--binary', str(self.binary), '--adapter', str(self.adapter),
                        '--fixture', 'first']
                if renderer == 'wined3d':
                    argv += ['--native-renderer', renderer]
                with patch('sys.argv', argv), \
                     patch.object(capture, 'prepare', side_effect=self.prepare), \
                     patch.object(capture, 'adapter_at', return_value=argparse.Namespace(PROTON=self.base)), \
                     patch.object(capture, 'run_pair', side_effect=self.pair):
                    self.assertEqual(self.sweep.main(), 0)
                self.assertEqual(json.loads((root / 'scenario.json').read_text())['native_renderer'], renderer)
                self.assertEqual(json.loads((root / 'sweep-state.json').read_text())['settings']['native_renderer'], renderer)

    def test_resume_cannot_change_native_renderer_or_reuse_wrong_backend_clips(self):
        self.run_fake(fixture=['first'], native_renderer='wined3d')
        previous = (self.base / 'results/report.json').read_bytes()
        with self.assertRaisesRegex(ValueError, 'matching resumed sweep'):
            self.run_fake(fixture=['first'], native_renderer='dxvk', resume=True)
        self.assertEqual(self.captured, ['first'])
        self.assertEqual((self.base / 'results/report.json').read_bytes(), previous)

    def test_fresh_dxvk_root_does_not_reuse_wined3d_results(self):
        self.run_fake(fixture=['first'], native_renderer='wined3d')
        report = self.run_fake(fixture=['first'], root=self.base / 'private-dxvk',
                               native_renderer='dxvk', resume=True)
        self.assertEqual(self.prepared, 2)
        self.assertEqual(self.captured, ['first', 'first'])
        self.assertFalse(report['results'][0]['resumed'])

    def test_native_volumetrics_quality_is_bound_to_sweep_owner(self):
        self.run_fake(fixture=['first'])
        owner = json.loads((self.base / 'private/sweep-state.json').read_text())
        self.assertEqual(owner['settings'].get('native_volumetrics'), 'ultra')

    def test_legacy_or_medium_quality_owner_is_rejected_without_rewriting(self):
        self.run_fake(fixture=['first'])
        path = self.base / 'private/sweep-state.json'
        original = json.loads(path.read_text())
        previous_report = (self.base / 'results/report.json').read_bytes()
        for quality in (None, 'medium'):
            with self.subTest(quality=quality):
                owner = json.loads(json.dumps(original))
                if quality is None:
                    owner['settings'].pop('native_volumetrics', None)
                else:
                    owner['settings']['native_volumetrics'] = quality
                write_json(path, owner)
                previous_owner = path.read_bytes()
                with self.assertRaisesRegex(ValueError, 'matching resumed sweep'):
                    self.run_fake(fixture=['first'], resume=True)
                self.assertEqual(path.read_bytes(), previous_owner)
                self.assertEqual((self.base / 'results/report.json').read_bytes(), previous_report)
        self.assertEqual(self.captured, ['first'])

    def test_actual_quality_must_match_even_when_scenario_claims_ultra(self):
        self.run_fake(fixture=['first'])
        path = self.base / 'private/app/config.json'
        for volumetrics, shadows in (('medium', 'medium'), ('ultra', 'ultra')):
            with self.subTest(volumetrics=volumetrics, shadows=shadows):
                write_json(path, {'steamuser': {'general': {'user': {
                    'volumetrics': volumetrics, 'shadows': shadows}}}})
                previous_config = path.read_bytes()
                report = self.run_fake(fixture=['first'], resume=True)
                self.assertEqual(report['results'][0]['status'], 'capture_failed')
                self.assertIn('quality', report['results'][0]['error'])
                self.assertEqual(path.read_bytes(), previous_config)
                self.assertEqual(self.captured, ['first'])

    def test_legacy_prepared_root_is_only_reusable_as_explicit_wined3d(self):
        self.run_fake(fixture=['first'], native_renderer='wined3d')
        root = self.base / 'private'
        owner = json.loads((root / 'sweep-state.json').read_text())
        owner['settings'].pop('native_renderer', None)
        write_json(root / 'sweep-state.json', owner)
        scenario = json.loads((root / 'scenario.json').read_text())
        scenario.pop('native_renderer', None)
        write_json(root / 'scenario.json', scenario)
        with self.assertRaisesRegex(ValueError, 'matching resumed sweep'):
            self.run_fake(fixture=['first'], resume=True)
        # Changed executable forces a new capture, so the legacy scenario must
        # actually be normalized before it reaches capture.run_pair.
        self.binary.write_bytes(b'candidate-v2')
        report = self.run_fake(fixture=['first'], native_renderer='wined3d', resume=True)
        self.assertEqual(report['results'][0]['status'], 'captured')
        self.assertEqual(json.loads((root / 'scenario.json').read_text())['native_renderer'], 'wined3d')
        self.assertEqual(self.captured, ['first', 'first'])

    def test_scenario_renderer_must_agree_with_owning_sweep(self):
        self.run_fake(fixture=['first'], native_renderer='dxvk')
        path = self.base / 'private/scenario.json'
        scenario = json.loads(path.read_text())
        scenario['native_renderer'] = 'wined3d'
        write_json(path, scenario)
        report = self.run_fake(fixture=['first'], native_renderer='dxvk', resume=True)
        self.assertEqual(report['results'][0]['status'], 'capture_failed')
        self.assertIn('renderer', report['results'][0]['error'])
        self.assertEqual(self.captured, ['first'])

    def test_content_mismatch_is_recorded_without_capturing_corrupted_fixture(self):
        Path(self.fixtures[1]['path'], 'scene.json').write_text('{}')
        report = self.run_fake()
        self.assertEqual(self.captured, ['first', 'last'])
        self.assertEqual(report['results'][1]['status'], 'capture_failed')
        self.assertIn('digest', report['results'][1]['error'])

    def test_subset_rejects_unknown_ids_and_does_not_capture_unrequested_rows(self):
        with self.assertRaisesRegex(ValueError, 'Unknown fixture'):
            self.run_fake(fixture=['missing'])
        report = self.run_fake(fixture=['last'])
        self.assertEqual(report['requested_ids'], ['last'])
        self.assertEqual(self.captured, ['last'])

    def test_preparation_failure_records_every_fixture(self):
        with patch.object(capture, 'prepare', side_effect=RuntimeError('prepare failed')):
            report = self.sweep.run_sweep(self.args())
        self.assertEqual(len(report['results']), 3)
        self.assertTrue(all(row['status'] == 'capture_failed' for row in report['results']))

    def test_resume_requires_unchanged_fixture_binary_library_and_artifacts(self):
        self.run_fake(fixture=['first'])
        report = self.run_fake(fixture=['first'], resume=True)
        self.assertTrue(report['results'][0]['resumed'])
        self.assertEqual(self.captured, ['first'])
        self.runtime.write_bytes(b'library-v2')
        self.run_fake(fixture=['first'], resume=True)
        self.assertEqual(self.captured, ['first', 'first'])
        self.binary.write_bytes(b'candidate-v2')
        self.run_fake(fixture=['first'], resume=True)
        self.assertEqual(self.captured, ['first', 'first', 'first'])
        latest = json.loads((self.base / 'results/first/latest.json').read_text())
        Path(latest['run'], 'worst-reference.png').write_bytes(b'broken image')
        self.run_fake(fixture=['first'], resume=True)
        self.assertEqual(self.captured, ['first', 'first', 'first', 'first'])
        self.assertEqual(self.prepared, 1)

    def test_corrupted_resume_record_is_recaptured_and_capture_code_changes_invalidate_it(self):
        self.run_fake(fixture=['first'])
        (self.base / 'results/first/latest.json').write_text('{truncated')
        report = self.run_fake(fixture=['first'], resume=True)
        self.assertEqual(report['results'][0]['status'], 'captured')
        self.assertEqual(self.captured, ['first', 'first'])
        with patch.object(capture, '__file__', str(self.adapter)):
            report = self.run_fake(fixture=['first'], resume=True)
        self.assertFalse(report['results'][0]['resumed'])
        self.assertEqual(self.captured, ['first', 'first', 'first'])

    def test_resume_recaptures_when_installed_dxvk_changes(self):
        self.run_fake(fixture=['first'])
        (self.base / 'files/lib/wine/dxvk/x86_64-windows/d3d11.dll').write_bytes(b'new DXVK version')
        report = self.run_fake(fixture=['first'], resume=True)
        self.assertFalse(report['results'][0]['resumed'])
        self.assertEqual(self.captured, ['first', 'first'])

    def test_resume_recaptures_when_artifact_records_are_malformed(self):
        self.run_fake(fixture=['first'])
        latest = self.base / 'results/first/latest.json'
        row = json.loads(latest.read_text())
        row['artifacts'] = 'invalid artifact list'
        write_json(latest, row)
        report = self.run_fake(fixture=['first'], resume=True)
        self.assertEqual(report['results'][0]['status'], 'captured')
        self.assertEqual(self.captured, ['first', 'first'])

    def test_failed_item_swap_preserves_state_for_later_fixtures(self):
        original_rename = Path.rename
        target = self.base / 'private/item'

        def rename(path, destination):
            proposed = path / 'scene.json'
            if destination == target and proposed.is_file() and json.loads(proposed.read_text())['name'] == 'broken':
                raise OSError('synthetic fixture rename failure')
            return original_rename(path, destination)

        with patch.object(Path, 'rename', rename):
            report = self.run_fake()
        self.assertEqual(self.captured, ['first', 'last'])
        self.assertEqual([row['status'] for row in report['results']], ['captured', 'capture_failed', 'captured'])
        self.assertIn('synthetic fixture rename failure', report['results'][1]['error'])
        self.assertEqual(json.loads((target / 'scene.json').read_text())['name'], 'last')

    def test_resume_recovers_interrupted_item_swap_before_reusing_capture(self):
        self.run_fake(fixture=['first'])
        root = self.base / 'private'
        old_item = (root / 'item/scene.json').read_bytes()
        old_controls = (root / 'controls.json').read_bytes()
        swap = root / '.fixture-swap'
        swap.mkdir()
        (root / 'item').rename(swap / 'previous-item')
        (root / 'item').mkdir()
        (root / 'item/scene.json').write_text('{"name":"interrupted"}')
        (root / 'controls.json').rename(swap / 'previous-controls.json')
        (root / 'controls.json').write_text('{}')
        (swap / 'next-scenario.json').write_text('{}')

        report = self.run_fake(fixture=['first'], resume=True)
        self.assertTrue(report['results'][0]['resumed'])
        self.assertFalse(swap.exists())
        self.assertEqual((root / 'item/scene.json').read_bytes(), old_item)
        self.assertEqual((root / 'controls.json').read_bytes(), old_controls)

    def test_originals_and_existing_unowned_private_root_are_never_deleted(self):
        root = self.base / 'private'
        root.mkdir()
        sentinel = root / 'keep'
        sentinel.write_text('untouched')
        with self.assertRaisesRegex(ValueError, 'owned|existing'):
            self.run_fake(resume=True)
        self.assertEqual(sentinel.read_text(), 'untouched')

    def test_outputs_inside_source_are_rejected_before_any_write(self):
        nested = Path(self.fixtures[0]['path']) / 'results'
        with self.assertRaisesRegex(ValueError, 'separate'):
            self.run_fake(out=nested)
        self.assertFalse(nested.exists())

    def test_mutated_private_inputs_are_not_deleted_for_the_next_fixture(self):
        original_pair = self.pair
        def mutation(args, scenario):
            result = original_pair(args, scenario)
            (args.root / 'item/scene.json').write_text('unexpected mutation')
            return result
        self.pair = mutation
        report = self.run_fake()
        self.assertEqual(self.captured, ['first'])
        self.assertEqual([row['status'] for row in report['results']],
                         ['captured', 'capture_failed', 'capture_failed'])
        self.assertEqual((self.base / 'private/item/scene.json').read_text(), 'unexpected mutation')

    def test_a_still_running_private_process_prevents_item_replacement(self):
        with patch.object(self.sweep, 'private_processes', create=True, return_value=[12345]):
            report = self.run_fake()
        self.assertEqual(self.captured, [])
        self.assertTrue(all('still running' in row['error'] for row in report['results']))
        self.assertEqual(json.loads((self.base / 'private/item/scene.json').read_text())['name'], 'first')

    def test_still_metrics_use_full_resolution_without_resize(self):
        ref, cand = self.base / 'ref.png', self.base / 'cand.png'
        Image.new('RGB', (2, 1), (0, 0, 0)).save(ref)
        Image.fromarray(np.array([[[3, 0, 0], [0, 0, 0]]], dtype=np.uint8)).save(cand)
        metrics = self.sweep.measure_still(ref, cand, self.base / 'diff.png', tolerance=2)
        self.assertEqual(metrics['mae_rgb8'], .5)
        self.assertAlmostEqual(metrics['rmse_rgb8'], 1.5 ** .5)
        self.assertEqual(metrics['changed_pixel_fraction'], .5)
        with Image.open(self.base / 'diff.png') as image:
            self.assertEqual(image.size, (2, 1))
        Image.new('RGB', (1, 1)).save(cand)
        with self.assertRaisesRegex(ValueError, 'geometry'):
            self.sweep.measure_still(ref, cand, self.base / 'diff.png')


if __name__ == '__main__':
    unittest.main()
