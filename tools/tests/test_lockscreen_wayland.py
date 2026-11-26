"""Opt-in native session-lock/feed regression in a private headless Sway session.

Run through dbus-run-session with LWE_TEST_WAYLAND_BINARY pointing at the engine.
The compositor, wallpaper socket, lock client and all display variables are private.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import tempfile
import time
import unittest


@unittest.skipUnless(os.environ.get('LWE_TEST_WAYLAND_BINARY'), 'Set LWE_TEST_WAYLAND_BINARY for headless Sway')
class NativeLockscreenFeed(unittest.TestCase):
    def test_animation_survives_session_lock_without_surface_swaps(self):
        root = Path(tempfile.mkdtemp(prefix='lwe-wayland-lock-'))
        runtime = root / 'runtime'
        runtime.mkdir(mode=0o700)
        env = dict(os.environ, XDG_RUNTIME_DIR=str(runtime), XDG_SESSION_TYPE='wayland',
                   XDG_CONFIG_HOME=str(root / 'config'), XDG_CACHE_HOME=str(root / 'cache'),
                   XDG_STATE_HOME=str(root / 'state'), XDG_DATA_HOME=str(root / 'data'),
                   XDG_CURRENT_DESKTOP='sway', WLR_BACKENDS='headless', WLR_RENDERER='pixman',
                   WLR_LIBINPUT_NO_DEVICES='1', LIBGL_ALWAYS_SOFTWARE='1', GALLIUM_DRIVER='llvmpipe',
                   __EGL_VENDOR_LIBRARY_FILENAMES='/usr/share/glvnd/egl_vendor.d/50_mesa.json',
                   QT_QPA_PLATFORM='wayland', QT_QUICK_BACKEND='software', SDL_AUDIODRIVER='dummy')
        for key in ('WAYLAND_DISPLAY', 'DISPLAY', 'HYPRLAND_INSTANCE_SIGNATURE', 'SWAYSOCK'):
            env.pop(key, None)
        # Refuse the ordinary desktop bus. The test command supplies a private one.
        self.assertIn('/tmp/', env.get('DBUS_SESSION_BUS_ADDRESS', ''), 'Run using dbus-run-session')
        config = root / 'sway.conf'
        config.write_text('xwayland disable\noutput HEADLESS-1 mode 640x360\nseat seat0 fallback true\n')
        scene = root / 'scene'
        scene.mkdir()
        (scene / 'project.json').write_text(json.dumps({'type': 'scene', 'file': 'scene.json', 'title': 'Private feed test'}))
        (scene / 'scene.json').write_text(json.dumps({
            'camera': {'eye': '0 0 1', 'center': '0 0 0', 'up': '0 1 0'},
            'general': {'orthogonalprojection': {'width': 320, 'height': 180},
                        'clearcolor': '0 0 0', 'bloom': False, 'camerafade': False},
            'objects': [{'id': 1, 'name': 'Animated red square', 'image': 'models/util/solidlayer.json',
                         'size': '60 60', 'color': '1 0 0',
                         'origin': {'value': '160 90 0', 'script':
                             'export function update() { return new Vec3(160+80*Math.sin(engine.runtime*4),90,0); }'}}]}))
        lock_dir = root / 'lock'
        lock_dir.mkdir()
        (lock_dir / 'shell.qml').write_text('''import QtQuick
import Quickshell
import Quickshell.Io
import Quickshell.Wayland
ShellRoot {
    WlSessionLock {
        id: lock
        WlSessionLockSurface { color: "black" }
    }
    IpcHandler {
        target: "fixture"
        function status(): string { return JSON.stringify({locked: lock.locked, secure: lock.secure}); }
        function engage(): string { lock.locked = true; return "ok"; }
        function release(): string { lock.locked = false; return "ok"; }
    }
}
''')
        processes = []
        files = []

        def launch(args, process_env, name):
            output = (root / (name + '.log')).open('w')
            files.append(output)
            process = subprocess.Popen(args, env=process_env, stdout=output, stderr=subprocess.STDOUT)
            processes.append(process)
            return process

        def wait_for(predicate, timeout=12):
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                result = predicate()
                if result:
                    return result
                time.sleep(.04)
            self.fail('Timed out; logs: ' + str(root))

        engine = None
        try:
            compositor = launch(['sway', '--config', str(config)], env, 'compositor')
            display = wait_for(lambda: next((p for p in runtime.glob('wayland-*')
                                            if p.is_socket()), None))
            env['WAYLAND_DISPLAY'] = display.name
            binary = Path(os.environ['LWE_TEST_WAYLAND_BINARY']).resolve()
            control = runtime / 'wallpaper.sock'
            engine_env = dict(env, WPE_CONTROL_SOCKET=str(control), WPE_LOG_FILE='off',
                              WAYLAND_DEBUG='client', LD_LIBRARY_PATH=str(binary.parent))
            engine = launch([str(binary), '--layer', 'background', '--screen-root', 'HEADLESS-1',
                             '--bg', str(scene), '--fps', '60', '--silent', '--noautomute',
                             '--disable-mouse', '--disable-parallax', '--no-audio-processing'], engine_env, 'engine')
            wait_for(control.exists, 25)

            def command(value):
                with socket.socket(socket.AF_UNIX) as client:
                    client.settimeout(3)
                    client.connect(str(control))
                    client.sendall((value + '\n').encode())
                    return client.recv(4096).decode()

            self.assertEqual(command('lockscreen-stream 30'), 'ok\n')
            frame = runtime / 'linux-wallpaperengine-lock/HEADLESS-1.bmp'
            wait_for(frame.exists)
            shell = launch(['qs', '-p', str(lock_dir)], env, 'lock')

            def ipc(action, required=True):
                result = subprocess.run(['qs', 'ipc', '--path', str(lock_dir), 'call', 'fixture', action],
                                        env=env, capture_output=True, text=True, timeout=3)
                if required:
                    self.assertEqual(result.returncode, 0, result.stderr + ' logs: ' + str(root))
                return result.stdout.strip() if result.returncode == 0 else ''

            wait_for(lambda: ipc('status', False))
            self.assertEqual(ipc('engage'), 'ok')
            wait_for(lambda: json.loads(ipc('status'))['secure'])
            time.sleep(.3)  # Drain the final callbacks issued before the lock took effect.
            log = root / 'engine.log'
            offset = log.stat().st_size
            pending_callback = re.findall(r'\.frame\(new id (wl_callback#\d+)\)', log.read_text())[-1]
            samples = []
            self.assertEqual(command('lockscreen-stream 30'), 'ok\n')
            for _ in range(8):
                time.sleep(.12)
                samples.append(hashlib.sha256(frame.read_bytes()).hexdigest())
            self.assertGreaterEqual(len(set(samples)), 4, 'Wallpaper stopped animating behind the lock')
            self.assertTrue(json.loads(ipc('status'))['secure'])
            hidden_protocol = log.read_bytes()[offset:].decode()
            self.assertNotIn('.frame(', hidden_protocol, 'Hidden wallpaper requested presentation callbacks')
            self.assertNotIn('.commit()', hidden_protocol, 'Hidden wallpaper committed a surface buffer')
            self.assertNotIn(pending_callback + '.done(', hidden_protocol, 'Wallpaper still received its frame callback')
            self.assertEqual(command('lockscreen-stream 0'), 'ok\n')
            stopped = frame.stat().st_mtime_ns
            time.sleep(.25)
            self.assertEqual(frame.stat().st_mtime_ns, stopped)
            self.assertEqual(command('lockscreen-stream 30'), 'ok\n')
            wait_for(lambda: frame.stat().st_mtime_ns != stopped)
            time.sleep(3.2)
            expired = frame.stat().st_mtime_ns
            time.sleep(.2)
            self.assertEqual(frame.stat().st_mtime_ns, expired)
            self.assertTrue(json.loads(ipc('status'))['secure'])
            offset = log.stat().st_size
            self.assertEqual(ipc('release'), 'ok')
            wait_for(lambda: not json.loads(ipc('status'))['secure'])
            wait_for(lambda: '.frame(' in log.read_bytes()[offset:].decode())
            engine.send_signal(signal.SIGINT)
            self.assertEqual(engine.wait(timeout=8), 0)
            self.assertNotIn('OpenGL error:', log.read_text())
            print(json.dumps({'result': 'PASS', 'logs': str(root), 'unique_locked_frames': len(set(samples)),
                              'hidden_surface_commits': 0, 'real_session_lock': True}), flush=True)
        finally:
            for process in reversed(processes):
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()
            for output in files:
                output.close()


if __name__ == '__main__':
    unittest.main()
