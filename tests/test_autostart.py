#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import importlib.util
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'kde/autostart'


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


restore = module('restore', SOURCE / 'restore-wallpapers.py')
installer = module('installer', SOURCE / 'install.py')


def record(screen, background):
    return {'screens': [screen], 'args': ['--screen-root', screen, '--bg', background, '--silent'], 'cwd': '/tmp'}


class AutostartTests(unittest.TestCase):
    def test_keep_selection_on_logout_and_replace_changed_screen(self):
        a, b, c = record('DP-1', 'a'), record('DP-2', 'b'), record('DP-1', 'c')
        self.assertEqual(restore.remember([a, b], []), [a, b])
        self.assertEqual(restore.remember([a, b], [c]), [c, b])

    def test_no_write_without_changes_and_final_snapshot(self):
        saved = [record('DP-1', 'a')]
        with patch.object(restore, 'running', return_value=saved), patch.object(restore, 'save_state') as save:
            self.assertEqual(restore.capture(saved), saved)
            save.assert_not_called()
            restore.capture(saved, force=True)
            save.assert_called_once_with(saved)

    def test_restore_missing_only_and_preserve_argv(self):
        a, b = record('DP-1', 'a'), record('DP-2', '/path with spaces;literal')
        with patch.object(restore, 'connected', return_value={'DP-1', 'DP-2'}), \
                patch.object(restore, 'running', return_value=[a]), \
                patch.object(restore.time, 'sleep'), patch.object(restore, 'spawn') as spawn:
            spawn.return_value.poll.return_value = None
            handled = set()
            self.assertFalse(restore.restore_pending([a, b], handled, {}, []))
            spawn.assert_called_once_with(b)
            self.assertEqual(handled, {'DP-1', 'DP-2'})
        with patch.object(restore.subprocess, 'Popen') as popen:
            restore.spawn(b)
            self.assertEqual(popen.call_args.args[0], [str(restore.BINARY), *b['args']])
            self.assertNotIn('shell', popen.call_args.kwargs)

    def test_disconnected_outputs_and_bounded_failure(self):
        saved = [record('DP-1', 'a')]
        with patch.object(restore, 'connected', return_value=set()), patch.object(restore, 'spawn') as spawn:
            self.assertTrue(restore.restore_pending(saved, set(), {}, []))
            spawn.assert_not_called()
        with patch.object(restore, 'connected', return_value={'DP-1'}), \
                patch.object(restore, 'running', return_value=[]), \
                patch.object(restore, 'spawn', side_effect=OSError('failed')) as spawn:
            attempts = {}
            for _ in range(5):
                restore.restore_pending(saved, set(), attempts, [])
            self.assertEqual(spawn.call_count, 3)

    def test_installer_is_repeatable_and_preserves_state_and_running_service(self):
        with tempfile.TemporaryDirectory(prefix='lwe setup ') as temp:
            root = Path(temp)
            renderer = root / 'custom % renderer'
            renderer.write_text('#!/bin/sh\nexit 0\n')
            renderer.chmod(0o700)
            state = root / 'state/last-wallpapers.json'
            state.parent.mkdir()
            state.write_text('existing selections')
            with patch.object(installer.Path, 'home', return_value=root), \
                    patch.dict(os.environ, {'XDG_CONFIG_HOME': str(root / 'config')}), \
                    patch.object(installer, 'run', return_value='') as run:
                installer.install(renderer)
                installer.install()  # uses saved binary, even without a PATH entry
                commands = [c.args for c in run.call_args_list]
                self.assertTrue(any('enable' in c for c in commands))
                self.assertFalse(any('restart' in c or 'stop' in c for c in commands))
            self.assertEqual(state.read_text(), 'existing selections')
            unit = (root / 'config/systemd/user' / installer.SERVICE).read_text()
            self.assertIn('KillMode=mixed', unit)
            self.assertIn('custom %% renderer', unit)
            self.assertIn('--binary', unit)
            self.assertEqual((root / '.local/libexec/linux-wallpaperengine/restore-wallpapers.py').read_bytes(),
                             (SOURCE / 'restore-wallpapers.py').read_bytes())

    def test_frontend_autostart_ownership_and_disabled_overrides(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            user, system = root / 'user', root / 'system'
            for base in (user, system):
                (base / 'autostart').mkdir(parents=True)
            frontend = user / 'Linux Wallpaper Engine'
            frontend.mkdir()
            (frontend / 'active-wallpapers.json').write_text(json.dumps({
                'activeWallpapers': {'DP-1': {}}, 'activePlaylists': {'DP-2': {}}}))
            entry = '[Desktop Entry]\nType=Application\nExec=linux-wallpaper-engine\n'
            (system / 'autostart/Linux Wallpaper Engine.desktop').write_text(entry)
            with patch.dict(os.environ, {'XDG_CONFIG_DIRS': str(system), 'XDG_CURRENT_DESKTOP': 'KDE'}), \
                    patch.object(restore.shutil, 'which', return_value='/usr/bin/linux-wallpaper-engine'):
                self.assertEqual(restore.frontend_autostart_screens(user), {'DP-1', 'DP-2'})
                override = user / 'autostart/Linux Wallpaper Engine.desktop'
                for extra in ('Hidden=true', 'OnlyShowIn=GNOME;', 'NotShowIn=KDE;',
                              'X-GNOME-Autostart-enabled=false', 'Exec=unrelated-app'):
                    override.write_text(entry + extra + '\n')
                    self.assertEqual(restore.frontend_autostart_screens(user), set(), extra)
                override.write_text(entry)
                with patch.object(restore.shutil, 'which', return_value=None):
                    self.assertEqual(restore.frontend_autostart_screens(user), set())
                (frontend / 'settings.json').write_text('{"windowMode":true}')
                self.assertEqual(restore.frontend_autostart_screens(user), set())
                (frontend / 'settings.json').unlink()
                (frontend / 'active-wallpapers.json').write_text('broken json')
                self.assertEqual(restore.frontend_autostart_screens(user), set())

    def test_delayed_frontend_and_failed_frontend_startup(self):
        # Run the real event loop and inotify. Only renderer processes and screen
        # discovery are simulated, so no desktop session is needed by the test.
        for frontend_starts in (True, False):
            with self.subTest(frontend_starts=frontend_starts), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                config = root / 'config'
                frontend = config / 'Linux Wallpaper Engine'
                frontend.mkdir(parents=True)
                active, launches = root / 'active.json', root / 'launches.json'
                active.write_text('[]')
                launches.write_text('[]')
                a, b = record('DP-1', 'a'), record('DP-2', 'b')
                fixture = root / 'fixture.py'
                fixture.write_text("""import importlib.util, json
from pathlib import Path
from types import SimpleNamespace
s=importlib.util.spec_from_file_location('r', SOURCE)
m=importlib.util.module_from_spec(s); s.loader.exec_module(m)
active, launches = Path(ACTIVE), Path(LAUNCHES)
m.running=lambda:json.loads(active.read_text())
m.connected=lambda:{'DP-1','DP-2'}
m.frontend_autostart_screens=lambda:{'DP-1'}
m.FRONTEND_STARTUP_GRACE=1.5
m.time.sleep=lambda _:None
def spawn(record):
 current=json.loads(active.read_text()); current.append(record)
 active.write_text(json.dumps(current))
 started=json.loads(launches.read_text()); started.extend(record['screens'])
 launches.write_text(json.dumps(started))
 return SimpleNamespace(poll=lambda:None)
m.spawn=spawn
m.monitor(RECORDS)
""".replace('SOURCE', repr(str(SOURCE / 'restore-wallpapers.py')))
                    .replace('ACTIVE', repr(str(active))).replace('LAUNCHES', repr(str(launches)))
                    .replace('RECORDS', repr([a, b])))
                log = root / 'log'
                with log.open('w') as output:
                    process = subprocess.Popen([sys.executable, str(fixture)],
                        env={**os.environ, 'XDG_CONFIG_HOME': str(config), 'XDG_STATE_HOME': str(root / 'state')},
                        stdout=output, stderr=subprocess.STDOUT)
                    try:
                        def wait_for(message):
                            deadline = time.monotonic() + 5
                            while time.monotonic() < deadline:
                                if message in log.read_text(): return
                                if process.poll() is not None: self.fail(log.read_text())
                                time.sleep(0.02)
                            self.fail('Timed out: ' + log.read_text())
                        wait_for('Restored wallpaper on DP-2')
                        self.assertEqual(json.loads(launches.read_text()), ['DP-2'])
                        if frontend_starts:
                            active.write_text(json.dumps([a, b]))
                            (frontend / 'active-wallpapers.json').write_text('{}')
                            wait_for('Frontend restored DP-1; no duplicate launch.')
                            time.sleep(1.6)  # beyond the fallback deadline
                            self.assertEqual(json.loads(launches.read_text()), ['DP-2'])
                            self.assertNotIn('timed out', log.read_text())
                        else:
                            wait_for('Restored wallpaper on DP-1')
                            self.assertEqual(json.loads(launches.read_text()), ['DP-2', 'DP-1'])
                            self.assertIn('Frontend startup timed out', log.read_text())
                        process.send_signal(signal.SIGTERM)
                        self.assertEqual(process.wait(timeout=5), 0, log.read_text())
                    finally:
                        if process.poll() is None:
                            process.kill()
                            process.wait()

    def test_event_notifications_idle_and_shutdown(self):
        with tempfile.TemporaryDirectory(prefix='lwe events ') as temp:
            root = Path(temp)
            config = root / 'config'
            frontend = config / 'Linux Wallpaper Engine'
            frontend.mkdir(parents=True)
            state = root / 'state/linux-wallpaperengine/last-wallpapers.json'
            active = root / 'fake-processes.json'
            counter = root / 'scan-counter'
            active.write_text(json.dumps([record('DP-1', 'first')]))
            fixture = root / 'fixture.py'
            fixture.write_text('''import importlib.util, json
from pathlib import Path
s=importlib.util.spec_from_file_location('r', SOURCE)
m=importlib.util.module_from_spec(s); s.loader.exec_module(m)
count=0
def running():
 global count
 count += 1
 Path(COUNTER).write_text(str(count))
 return json.loads(Path(ACTIVE).read_text())
m.running=running
m.connected=lambda:set()
m.monitor(m.load_state())
'''.replace('SOURCE', repr(str(SOURCE / 'restore-wallpapers.py')))
                .replace('COUNTER', repr(str(counter))).replace('ACTIVE', repr(str(active))))
            log = root / 'log'
            with log.open('w') as output:
                process = subprocess.Popen([sys.executable, str(fixture)],
                    env={**os.environ, 'XDG_CONFIG_HOME': str(config), 'XDG_STATE_HOME': str(root / 'state')},
                    stdout=output, stderr=subprocess.STDOUT)
                try:
                    def wait_for(predicate):
                        deadline = time.monotonic() + 5
                        while time.monotonic() < deadline:
                            if predicate(): return
                            if process.poll() is not None: self.fail(log.read_text())
                            time.sleep(0.03)
                        self.fail('Timed out: ' + log.read_text())
                    wait_for(lambda: state.exists() and 'Event-driven' in log.read_text())
                    time.sleep(0.1)
                    scans = counter.read_text()
                    modified = state.stat().st_mtime_ns
                    time.sleep(2.3)
                    self.assertEqual(counter.read_text(), scans, 'idle tracker must not scan processes')
                    self.assertEqual(state.stat().st_mtime_ns, modified)
                    (frontend / 'settings.json').write_text('{}')
                    wait_for(lambda: counter.read_text() != scans)
                    self.assertEqual(state.stat().st_mtime_ns, modified, 'unchanged selection must not be rewritten')
                    active.write_text(json.dumps([record('DP-1', 'changed')]))
                    tmp = frontend / 'new.json'
                    tmp.write_text('{}')
                    tmp.replace(frontend / 'active-wallpapers.json')
                    wait_for(lambda: 'changed' in state.read_text())
                    # Final scan catches a change even without a configuration event.
                    active.write_text(json.dumps([record('DP-1', 'shutdown-choice')]))
                    process.send_signal(signal.SIGTERM)
                    self.assertEqual(process.wait(timeout=5), 0, log.read_text())
                    self.assertIn('shutdown-choice', state.read_text())
                finally:
                    if process.poll() is None:
                        process.kill()
                        process.wait()


if __name__ == '__main__':
    unittest.main(verbosity=2)
