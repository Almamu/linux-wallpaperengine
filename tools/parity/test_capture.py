#!/usr/bin/env python3
"""Containment controls: run without starting a display or renderer."""
import json
import os
from pathlib import Path
import tempfile
import time
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

import capture


class CaptureTests(unittest.TestCase):
    def test_prepare_pins_native_volume_and_shadow_quality_without_changing_original(self):
        with tempfile.TemporaryDirectory(prefix='parity-quality-') as directory:
            base = Path(directory)
            original = base / 'original-config.json'
            original.write_text(json.dumps({'steamuser': {'general': {'user': {
                'volumetrics': 'medium', 'shadows': 'ultra', 'preset': 'medium',
                'resolution': 'full'}}, 'wproperties': {'previous': True}}}))
            before = original.read_bytes()
            item = base / 'fixture'
            item.mkdir()
            (item / 'project.json').write_text(json.dumps({'type': 'scene', 'title': 'Quality fixture'}))
            adapter_path = base / 'adapter.py'
            adapter_path.write_text('# CPU-only private adapter fixture\n')
            root = base / 'private'

            def prepare_private(private, width, height):
                (private / 'app').mkdir(parents=True)
                (private / 'app/config.json').write_bytes(original.read_bytes())

            args = SimpleNamespace(root=root, item=item, adapter=adapter_path, properties=None,
                width=320, height=180, fps=10, post_processing='enabled', native_renderer='dxvk')
            with patch.object(capture, 'adapter_at', return_value=SimpleNamespace(prepare=prepare_private)):
                scenario = capture.prepare(args)
            self.assertEqual(original.read_bytes(), before)
            app_config = json.loads((root / 'app/config.json').read_text())
            self.assertEqual(json.loads((root / 'capture-config.json').read_text()), app_config)
            user = app_config['steamuser']['general']['user']
            self.assertEqual(user['volumetrics'], 'ultra')
            self.assertEqual(user['shadows'], 'medium')
            self.assertEqual(user['preset'], 'medium')
            self.assertEqual(user['resolution'], 'full')
            self.assertEqual(scenario['native_volumetrics'], 'ultra')
            self.assertEqual(scenario['config_sha256'], capture.sha256(root / 'app/config.json'))

    def test_original_and_bare_tmp_are_rejected(self):
        for path in ('/tmp', '/home/admin/.local/share/Steam', '/'):
            with self.assertRaises(ValueError):
                capture.private_root(Path(path))
        self.assertEqual(capture.private_root(Path('/tmp/parity-unit')), Path('/tmp/parity-unit'))

    def test_prepared_quality_must_match_config_and_scenario_before_capture(self):
        self.assertTrue(hasattr(capture, 'validate_native_quality'))
        with tempfile.TemporaryDirectory(prefix='parity-quality-check-') as directory:
            root = Path(directory)
            (root / 'app').mkdir()
            scenario = {'native_volumetrics': 'ultra'}
            for volumetrics, shadows in (('medium', 'medium'), ('ultra', 'ultra')):
                (root / 'app/config.json').write_text(json.dumps({'steamuser': {'general': {'user': {
                    'volumetrics': volumetrics, 'shadows': shadows}}}}))
                args = SimpleNamespace(root=root, out=root / 'out')
                with self.assertRaisesRegex(ValueError, 'quality'):
                    capture.run_pair(args, dict(scenario, native_renderer='dxvk'))
                self.assertFalse(args.out.exists())
            (root / 'app/config.json').write_text(json.dumps({'steamuser': {'general': {'user': {
                'volumetrics': 'ultra', 'shadows': 'medium'}}}}))
            capture.validate_native_quality(root, scenario)
            with self.assertRaisesRegex(ValueError, 'quality'):
                capture.validate_native_quality(root, {})

    def test_private_launch_cannot_inherit_host_display_audio_or_socket(self):
        with tempfile.TemporaryDirectory(prefix='parity-runtime-') as directory:
            root = Path(directory)
            env = capture.private_environment({'DISPLAY': ':1', 'WAYLAND_DISPLAY': 'wayland-1',
                'LD_PRELOAD': '/host/inject.so', 'WPE_CONTROL_SOCKET': '/tmp/live.sock',
                'XDG_RUNTIME_DIR': '/run/user/1000',
                'DBUS_SESSION_BUS_ADDRESS': 'unix:path=/run/user/1000/bus',
                'PULSE_SERVER': 'unix:/run/user/1000/pulse/native'}, root)
            self.assertEqual(env['XDG_RUNTIME_DIR'], str(root / 'runtime'))
            self.assertEqual((root / 'runtime').stat().st_mode & 0o777, 0o700)
        self.assertNotIn('DISPLAY', env)
        self.assertNotIn('WAYLAND_DISPLAY', env)
        self.assertNotIn('LD_PRELOAD', env)
        self.assertNotIn('DBUS_SESSION_BUS_ADDRESS', env)
        self.assertNotEqual(env['WPE_CONTROL_SOCKET'], '/tmp/live.sock')
        self.assertNotEqual(env['PULSE_SERVER'], 'unix:/run/user/1000/pulse/native')
        self.assertEqual(env['LWE_HOST_DISPLAY'], ':1')

    def test_headless_launch_creates_a_private_bus_before_gamescope(self):
        with tempfile.TemporaryDirectory(prefix='parity-launch-') as directory:
            root = Path(directory)
            (root / 'app/assets').mkdir(parents=True)
            (root / 'app/config.json').write_text(json.dumps({'steamuser': {'general': {'user': {
                'volumetrics': 'ultra', 'shadows': 'medium'}}}}))
            (root / 'app/wallpaper64.exe').write_bytes(b'native')
            (root / 'candidate').write_bytes(b'candidate')
            (root / 'proton').write_bytes(b'proton')
            adapter = root / 'adapter.py'
            adapter.write_text(f'from pathlib import Path\nPROTON = Path({str(root)!r})\n')
            scenario = dict(item=dict(id='fixture', project_sha256='a', asset_digest='b'),
                            adapter=dict(path=str(adapter), sha256=capture.sha256(adapter)),
                            width=320, height=180, native_renderer='dxvk', native_volumetrics='ultra')
            args = SimpleNamespace(root=root, out=root / 'out', binary=root / 'candidate',
                                   diagnostic=True, duration=1, warmup=0, startup_timeout=1)
            with patch.object(capture, 'LOCK', str(root / 'capture.lock')), patch.object(
                    capture.subprocess, 'Popen', side_effect=RuntimeError('launch intercepted')) as launch:
                result = capture.run_pair(args, scenario)
            command = launch.call_args.args[0]
            self.assertEqual(command[:6], ['dbus-run-session', '--', 'gamescope',
                                          '--backend', 'headless', '--disable-color-management'])
            self.assertEqual(json.loads((args.out / 'reference-headless-command.json').read_text()), command)
            self.assertEqual(result['blockers'], ['launch intercepted'])

    def test_native_renderer_overrides_conflicting_inherited_backend(self):
        adapter = SimpleNamespace(STEAM=Path('/steam'))
        inherited = {'WINEDLLOVERRIDES': 'd3d11,dxgi=b', 'PROTON_USE_WINED3D': '1',
                     'PROTON_USE_WINED3D11': '1'}
        env = capture.native_environment(inherited, Path('/tmp/private'), adapter)
        self.assertEqual(env['WINEDLLOVERRIDES'], 'd3d11,dxgi=n,b')
        self.assertEqual(env['PROTON_USE_WINED3D'], '0')
        self.assertEqual(env['PROTON_USE_WINED3D11'], '0')
        env = capture.native_environment(inherited, Path('/tmp/private'), adapter, 'wined3d')
        self.assertEqual(env['WINEDLLOVERRIDES'], 'd3d11,dxgi=b')
        self.assertEqual(env['PROTON_USE_WINED3D'], '1')
        with self.assertRaises(ValueError):
            capture.native_environment(inherited, Path('/tmp/private'), adapter, None)

    def test_native_renderer_requires_expected_loaded_library_paths_and_hashes(self):
        self.assertTrue(hasattr(capture, 'verify_native_renderer'), 'Loaded native backend must be verified')
        with tempfile.TemporaryDirectory(prefix='parity-backend-') as directory:
            root = Path(directory)
            adapter = SimpleNamespace(PROTON=root / 'proton')
            private = root / 'compat/pfx/drive_c/windows/system32'
            dxvk = adapter.PROTON / 'files/lib/wine/dxvk/x86_64-windows'
            builtin = adapter.PROTON / 'files/lib/wine/x86_64-windows'
            for path in (private, dxvk, builtin):
                path.mkdir(parents=True)
            for name in ('d3d11.dll', 'dxgi.dll'):
                (private / name).write_text('dxvk ' + name)
                (dxvk / name).write_text('dxvk ' + name)
            for name in ('d3d11.dll', 'dxgi.dll', 'wined3d.dll'):
                (builtin / name).write_text('builtin ' + name)
            dxvk_maps = [dict(path=str(path), sha256=capture.sha256(path)) for path in sorted(private.iterdir())]
            wine_maps = [dict(path=str(path), sha256=capture.sha256(path)) for path in sorted(builtin.iterdir())]
            capture.verify_native_renderer(dxvk_maps, 'dxvk', root, adapter)
            capture.verify_native_renderer(wine_maps, 'wined3d', root, adapter)
            for loaded in (wine_maps, dxvk_maps[:1], dxvk_maps + wine_maps):
                with self.subTest(loaded=loaded), self.assertRaises(RuntimeError):
                    capture.verify_native_renderer(loaded, 'dxvk', root, adapter)
            dxvk_maps[0]['sha256'] = 'unexpected old DLL'
            with self.assertRaises(RuntimeError):
                capture.verify_native_renderer(dxvk_maps, 'dxvk', root, adapter)

    def test_wined3d_verifies_resolved_proton_paths_and_identical_private_builtins(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            proton = root / 'proton'
            alias = root / 'proton-link'
            alias.symlink_to(proton, target_is_directory=True)
            builtin = proton / 'files/lib/wine/x86_64-windows'
            private = root / 'compat/pfx/drive_c/windows/system32'
            for path in (builtin, private):
                path.mkdir(parents=True)
                for name in ('d3d11.dll', 'dxgi.dll', 'wined3d.dll'):
                    (path / name).write_bytes(name.encode())
            adapter = SimpleNamespace(PROTON=alias)
            for path in (builtin, private):
                mapped = [dict(path=str(dll), sha256=capture.sha256(dll)) for dll in sorted(path.iterdir())]
                self.assertEqual(capture.verify_native_renderer(mapped, 'wined3d', root, adapter)['libraries'], mapped)

    def test_scene_readiness_ignores_stale_and_other_markers(self):
        self.assertTrue(hasattr(capture, 'wait_scene_ready'), 'Scene update readiness is required')
        marker = 'LWE_FIXTURE_READY_example'
        with tempfile.TemporaryDirectory(prefix='parity-ready-') as directory:
            path = Path(directory) / 'app.log'
            path.write_text(f'Error: {marker} {{"runtime":0.1,"at":1000}}\n')
            before = path.stat()
            with path.open('a') as stream:
                stream.write(f'Error: {marker}_other {{"runtime":0.2,"at":2000}}\n')
                stream.write(f'Error: prefix{marker} {{"runtime":0.2,"at":2000}}\n')
            proc = Mock()
            proc.poll.return_value = None
            with self.assertRaises(TimeoutError):
                capture.wait_scene_ready(path, marker, proc, 'reference', time.monotonic() + .02,
                                         before.st_size, before.st_ino)
            with path.open('a') as stream:
                stream.write(f'Error: {marker} {{"runtime":0.25,"at":3000}}\n')
            result = capture.wait_scene_ready(path, marker, proc, 'reference', time.monotonic() + .2,
                                              before.st_size, before.st_ino)
            self.assertEqual(result, {'runtime': 0.25, 'at': 3000})

    def test_scene_readiness_handles_rotated_logs_and_candidate_error_format(self):
        self.assertTrue(hasattr(capture, 'wait_scene_ready'), 'Scene update readiness is required')
        marker = 'LWE_FIXTURE_READY_example'
        with tempfile.TemporaryDirectory(prefix='parity-ready-') as directory:
            path = Path(directory) / 'engine.log'
            path.write_text('old log without readiness' * 10)
            before = path.stat()
            path.rename(path.with_suffix('.old'))
            path.write_text(f'19:01:02.123 [ERROR] {marker} {{"runtime":0,"at":4000}}\n')
            proc = Mock()
            proc.poll.return_value = None
            result = capture.wait_scene_ready(path, marker, proc, 'candidate', time.monotonic() + .2,
                                              before.st_size, before.st_ino)
            self.assertEqual(result, {'runtime': 0, 'at': 4000})

    def test_reference_worker_observes_first_update_when_app_rewrites_its_log(self):
        with tempfile.TemporaryDirectory() as directory:
            root, marker = Path(directory), 'LWE_FIXTURE_READY_rewritten'
            out = root / 'out'
            for path in (root / 'app', root / 'item', out):
                path.mkdir()
            (root / 'app/config.json').write_text(json.dumps({'steamuser': {'general': {'user': {
                'volumetrics': 'ultra', 'shadows': 'medium'}}}}))
            native_log = root / 'app/log.txt'
            native_log.write_text('old log' * 20)
            (out / 'scenario.json').write_text('{}')
            scenario = dict(adapter={'path': 'unused'}, width=320, height=180, fps=10,
                            native_renderer='dxvk', native_volumetrics='ultra', captured_files=[],
                            fixture={'ready_marker': marker})
            args = SimpleNamespace(root=root, out=out, worker='reference', startup_timeout=.1, warmup=0)
            proc = Mock()
            proc.poll.return_value = None

            def launch(*unused_args, **unused_kwargs):
                native_log.write_text(f'{marker} {{"runtime":0.25,"at":3000}}\n' + 'new log' * 30)
                return proc

            adapter = Mock(PROTON=root, STEAM=root, win=str)
            adapter.X11.return_value.size.return_value = [320, 180]
            with patch.dict(os.environ, {'DISPLAY': ':8'}, clear=True), \
                 patch.object(capture, 'adapter_at', return_value=adapter), \
                 patch.object(capture, 'owned_window', return_value=('0x123', 123)), \
                 patch.object(capture.subprocess, 'Popen', side_effect=launch), \
                 patch.object(capture, 'mapped_identity', side_effect=RuntimeError('capture intercepted')), \
                 patch.object(capture, 'stop_native', return_value=0), patch.object(capture, 'stop_group'):
                result = capture.worker(args, scenario)
            self.assertEqual(result.get('error'), 'capture intercepted')
            self.assertEqual(result['scene_ready']['payload'], {'runtime': .25, 'at': 3000})

    def test_scene_readiness_times_out_without_a_log_and_rejects_invalid_payload(self):
        self.assertTrue(hasattr(capture, 'wait_scene_ready'), 'Scene update readiness is required')
        marker = 'LWE_FIXTURE_READY_example'
        with tempfile.TemporaryDirectory(prefix='parity-ready-') as directory:
            path = Path(directory) / 'missing.log'
            proc = Mock()
            proc.poll.return_value = 0  # Proton can exit while its owned app remains alive.
            with self.assertRaises(TimeoutError):
                capture.wait_scene_ready(path, marker, proc, 'reference', time.monotonic() + .02)
            path.write_text(f'Error: {marker} {{"runtime":true,"at":4000}}\n')
            with self.assertRaises(ValueError):
                capture.wait_scene_ready(path, marker, proc, 'reference', time.monotonic() + .2)
            with self.assertRaises(RuntimeError):
                capture.wait_scene_ready(path, marker, proc, 'candidate', time.monotonic() + .2)

    def test_properties_are_validated_before_private_project_is_changed(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'project.json'
            original = json.dumps({'general': {'properties': {'camera': {'value': 0, 'type': 'combo'}}}})
            path.write_text(original)
            with self.assertRaises(ValueError):
                capture.set_properties(path, {'camera': 1, 'missing': True})
            self.assertEqual(path.read_text(), original)
            capture.set_properties(path, {'camera': 2})
            self.assertEqual(json.loads(path.read_text())['general']['properties']['camera']['value'], 2)

    def test_capture_command_targets_exact_window_and_keeps_rgb(self):
        cmd = capture.video_command(':7', '0x123', 320, 180, 30, 90, Path('/tmp/clip.mkv'))
        self.assertEqual(cmd[cmd.index('-window_id') + 1], '291')
        self.assertEqual(cmd[cmd.index('-i') + 1], ':7')
        self.assertEqual(cmd[cmd.index('-c:v') + 1], 'ffv1')
        self.assertEqual(cmd[cmd.index('-pix_fmt') + 1], 'bgr0')
        self.assertEqual(cmd[cmd.index('-fps_mode') + 1], 'passthrough')
        self.assertNotIn('-r', cmd)
        self.assertNotIn('-vf', cmd)

    def test_prefix_ownership_accepts_proton_trailing_slash_but_not_siblings(self):
        self.assertTrue(capture.owns_prefix([b'WINEPREFIX=/tmp/owned/compat/pfx/'], Path('/tmp/owned')))
        self.assertFalse(capture.owns_prefix([b'WINEPREFIX=/tmp/owned-other/compat/pfx/'], Path('/tmp/owned')))

    def test_reference_window_disappearing_during_ownership_check_is_skipped(self):
        x11 = Mock()
        x11.named.return_value = ['0x101', '0x102']
        with patch.object(capture.subprocess, 'run', side_effect=[
            Mock(stdout='_NET_WM_PID(CARDINAL) = 101'),
            Mock(stdout='_NET_WM_PID(CARDINAL) = 102'),
        ]), patch.object(Path, 'read_bytes', side_effect=[
            ProcessLookupError('process exited'), b'WINEPREFIX=/tmp/owned/compat/pfx/\0',
        ]):
            self.assertEqual(capture.owned_window(x11, Path('/tmp/owned'), 'reference', Mock()),
                             ('0x102', 102))

    def test_reference_window_with_foreign_prefix_is_rejected(self):
        x11 = Mock()
        x11.named.return_value = ['0x101']
        with patch.object(capture.subprocess, 'run', return_value=Mock(
            stdout='_NET_WM_PID(CARDINAL) = 101')), patch.object(Path, 'read_bytes',
            return_value=b'WINEPREFIX=/tmp/foreign/compat/pfx/\0'):
            with self.assertRaisesRegex(RuntimeError, 'Refusing unowned'):
                capture.owned_window(x11, Path('/tmp/owned'), 'reference', Mock())

    def test_input_boundary_distinguishes_generated_cache_from_authored_mutation(self):
        with tempfile.TemporaryDirectory() as directory:
            item = Path(directory)
            (item / 'project.json').write_text('{}')
            expected = capture.source_manifest(item)
            cache = item / 'shaders/blobsSM40/123abc.dxs'
            cache.parent.mkdir(parents=True)
            cache.write_bytes(b'compiled shader')
            generated = capture.check_input_boundary(item, expected)
            self.assertEqual([record['path'] for record in generated], ['shaders/blobsSM40/123abc.dxs'])
            (item / 'project.json').write_text('{"changed":true}')
            with self.assertRaises(ValueError):
                capture.check_input_boundary(item, expected)


if __name__ == '__main__':
    unittest.main()
