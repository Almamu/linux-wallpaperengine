#!/usr/bin/env python3
"""Native preparation and display isolation, without starting an application."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import capture


class NativeAdapterTests(unittest.TestCase):
    def adapter(self):
        self.assertTrue(capture.DEFAULT_ADAPTER.is_file(), 'Default capture adapter must be shipped')
        return capture.adapter_at(capture.DEFAULT_ADAPTER)

    def test_prepare_preserves_originals_and_omits_selector_and_external_prefix_links(self):
        adapter = self.adapter()
        with tempfile.TemporaryDirectory(prefix='parity-adapter-', dir='/tmp') as directory:
            base = Path(directory)
            app, compat, proton = (base / name for name in ('source-app', 'source-compat', 'proton'))
            (app / 'bin').mkdir(parents=True)
            (app / 'assets').mkdir()
            (app / 'wallpaper64.exe').write_bytes(b'native executable')
            (app / 'bin/wallpaperui.exe').write_bytes(b'selector')
            (app / 'assets/test.tex').write_bytes(b'original asset')
            config = {'?installdirectory': 'Z:/original', 'steamuser': {'general': {
                'user': {'fps': 61, 'postprocessing': 'ultra', 'autostart': True},
                'wallpaperconfig': {'selectedwallpapers': {'old': 'original'}}}}}
            (app / 'config.json').write_text(json.dumps(config))
            (compat / 'pfx/dosdevices').mkdir(parents=True)
            (compat / 'pfx/drive_c').mkdir()
            (compat / 'pfx/user.reg').write_text('original registry')
            (compat / 'pfx/dosdevices/c:').symlink_to('../drive_c')
            (compat / 'pfx/dosdevices/z:').symlink_to('/')
            (compat / 'pfx/dosdevices/s:').symlink_to(base / 'source-steam')
            (compat / 'pfx/dosdevices/com1').symlink_to('/dev/ttyS0')
            proton.mkdir()
            (proton / 'proton').write_text('runtime')
            (proton / 'runtime.dll').write_bytes(b'runtime dll')
            (compat / 'pfx/drive_c/runtime.dll').symlink_to(proton / 'runtime.dll')
            dxvk = proton / 'files/lib/wine/dxvk/x86_64-windows'
            dxvk.mkdir(parents=True)
            (dxvk / 'd3d11.dll').write_bytes(b'current dxvk d3d11')
            (dxvk / 'dxgi.dll').write_bytes(b'current dxvk dxgi')
            system32 = compat / 'pfx/drive_c/windows/system32'
            system32.mkdir(parents=True)
            (system32 / 'd3d11.dll').symlink_to(proton / 'runtime.dll')
            (system32 / 'dxgi.dll').write_bytes(b'old prefix dxgi')
            root = base / 'private'
            with patch.multiple(adapter, APP=app, COMPAT=compat, PROTON=proton):
                adapter.prepare(root, 320, 180)
            self.assertEqual(root.stat().st_mode & 0o777, 0o700)
            self.assertFalse((root / 'app/bin/wallpaperui.exe').exists())
            self.assertTrue((app / 'bin/wallpaperui.exe').exists())
            self.assertFalse((root / 'compat/pfx/dosdevices/s:').is_symlink())
            self.assertFalse((root / 'compat/pfx/dosdevices/com1').is_symlink())
            self.assertEqual((root / 'compat/pfx/dosdevices/c:').resolve(), root / 'compat/pfx/drive_c')
            self.assertEqual((root / 'compat/pfx/drive_c/runtime.dll').read_bytes(), b'runtime dll')
            private_system32 = root / 'compat/pfx/drive_c/windows/system32'
            self.assertEqual((private_system32 / 'd3d11.dll').read_bytes(), b'current dxvk d3d11')
            self.assertFalse((private_system32 / 'd3d11.dll').is_symlink())
            self.assertEqual((private_system32 / 'dxgi.dll').read_bytes(), b'current dxvk dxgi')
            self.assertEqual((system32 / 'dxgi.dll').read_bytes(), b'old prefix dxgi')
            self.assertEqual((proton / 'runtime.dll').read_bytes(), b'runtime dll')
            copied = json.loads((root / 'app/config.json').read_text())
            self.assertEqual(copied['?installdirectory'], 'Z:' + str(root / 'app').replace('/', '\\'))
            self.assertEqual(copied['steamuser']['general']['user']['fps'], 61)
            self.assertEqual(copied['steamuser']['general']['user']['postprocessing'], 'ultra')
            (root / 'app/assets/test.tex').write_bytes(b'private edit')
            (root / 'compat/pfx/user.reg').write_text('private edit')
            self.assertEqual((app / 'assets/test.tex').read_bytes(), b'original asset')
            self.assertEqual((compat / 'pfx/user.reg').read_text(), 'original registry')
            self.assertEqual(json.loads((app / 'config.json').read_text()), config)

    def test_prepare_rejects_existing_root_and_paths_outside_tmp(self):
        adapter = self.adapter()
        with tempfile.TemporaryDirectory(prefix='parity-adapter-', dir='/tmp') as directory:
            for root in (Path(directory), Path('/tmp'), Path.home() / 'parity-rejected'):
                with self.subTest(root=root), self.assertRaises((ValueError, FileExistsError)):
                    adapter.prepare(root, 320, 180)

    def test_prepare_never_replaces_dlls_in_a_linked_proton_directory(self):
        adapter = self.adapter()
        with tempfile.TemporaryDirectory(dir='/tmp') as directory:
            base = Path(directory)
            app, compat, proton = (base / name for name in ('app', 'compat', 'proton'))
            app.mkdir()
            (app / 'wallpaper64.exe').touch()
            (app / 'config.json').write_text('{}')
            windows = compat / 'pfx/drive_c/windows'
            windows.mkdir(parents=True)
            (compat / 'pfx/user.reg').touch()
            dxvk = proton / 'files/lib/wine/dxvk/x86_64-windows'
            dxvk.mkdir(parents=True)
            (proton / 'proton').touch()
            for name in ('d3d11.dll', 'dxgi.dll'):
                (dxvk / name).write_bytes(b'installed Proton DLL')
            (windows / 'system32').symlink_to(dxvk, target_is_directory=True)

            with patch.multiple(adapter, APP=app, COMPAT=compat, PROTON=proton):
                with self.assertRaisesRegex(ValueError, 'system32'):
                    adapter.prepare(base / 'private', 320, 180)
            for name in ('d3d11.dll', 'dxgi.dll'):
                self.assertEqual((dxvk / name).read_bytes(), b'installed Proton DLL')

    def test_display_requires_distinct_local_x_server(self):
        adapter = self.adapter()
        for display, host in [('', ':1'), (':1', ':1'), (':1.0', ':1'),
                              (':1', ':1.0'), ('localhost:1', ':1'), ('remote:9', ':1')]:
            with self.subTest(display=display, host=host), self.assertRaises(ValueError):
                adapter.validate_display(display, host)
        adapter.validate_display(':8', ':1')
        adapter.validate_display(':8.0', '')


if __name__ == '__main__':
    unittest.main()
