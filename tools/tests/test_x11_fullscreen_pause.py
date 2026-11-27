"""Exercise fullscreen pause on a private X11 display with one active output."""
from contextlib import closing
import importlib.util
import json
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import time
import unittest


MARKER = 'X11_PAUSE_FRAME'


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY') and importlib.util.find_spec('Xlib'),
                     'Set LWE_TEST_BINARY and install python-xlib for the private X11 test')
class X11FullscreenPause(unittest.TestCase):
    def test_pauses_only_for_a_fullscreen_cover_when_enabled(self):
        for pause, desktop_mode in ((True, False), (False, False), (True, True)):
            with self.subTest(pause=pause, desktop_mode=desktop_mode):
                self.check_cover(pause, desktop_mode)

    def check_cover(self, pause, desktop_mode):
        from Xlib import X, Xatom, display
        from Xlib.ext import randr

        self.assertTrue(os.environ.get('DISPLAY'))
        self.assertIn('LWE_HOST_DISPLAY', os.environ)
        self.assertNotEqual(os.environ['DISPLAY'], os.environ['LWE_HOST_DISPLAY'])
        with tempfile.TemporaryDirectory(prefix='lwe-x11-fullscreen-') as directory, closing(display.Display()) as private:
            screen = private.screen()
            root = screen.root
            monitor = root.get_geometry()
            width, height = monitor.width, monitor.height
            resources = root.xrandr_get_screen_resources_current()
            crtcs = [randr.get_crtc_info(private, crtc, resources.config_timestamp) for crtc in resources.crtcs]
            self.assertEqual([(crtc.x, crtc.y, crtc.width, crtc.height) for crtc in crtcs if crtc.mode],
                             [(0, 0, width, height)])
            if desktop_mode:
                active = next(crtc for crtc in crtcs if crtc.mode)
                info = randr.get_output_info(private, active.outputs[0], resources.config_timestamp)
                output_name = info.name.decode() if isinstance(info.name, bytes) else info.name
                mode = ['--screen-root', output_name]
            else:
                mode = ['--window', f'0x0x{width}x{height}']

            scene = Path(directory)
            (scene / 'project.json').write_text(json.dumps({'type': 'scene', 'file': 'scene.json', 'title': 'X11 probe'}))
            (scene / 'scene.json').write_text(json.dumps({
                'camera': {'eye': '0 0 1', 'center': '0 0 0', 'up': '0 1 0'},
                'general': {'orthogonalprojection': {'width': width, 'height': height},
                            'camerafade': False, 'clearcolor': '0 0 0'},
                'objects': [{'id': 1, 'name': 'probe', 'solid': True, 'visible': {'value': True, 'script':
                             f"export function update(value) {{ console.log('{MARKER}'); return value; }}"}}],
            }))
            path = scene / 'engine.log'
            env = dict(os.environ, XDG_SESSION_TYPE='x11', WPE_LOG_FILE='off',
                       WPE_CONTROL_SOCKET=str(scene / 'control.sock'))
            env.pop('WAYLAND_DISPLAY', None)
            binary = Path(os.environ['LWE_TEST_BINARY']).resolve()
            assets = os.environ.get('LWE_TEST_ASSETS')
            options = ['--assets-dir', assets] if assets else []
            if not pause:
                options.append('--no-fullscreen-pause')
            with path.open('w') as log:
                engine = subprocess.Popen([str(binary), *mode,
                                           '--silent', '--no-audio-processing', '--fps', '25',
                                           *options, str(scene)], env=env, stdout=log, stderr=subprocess.STDOUT)
                try:
                    def output():
                        return path.read_text()

                    def samples():
                        return output().count(MARKER)

                    def progress(label):
                        before = samples()
                        deadline = time.monotonic() + 2
                        while engine.poll() is None and time.monotonic() < deadline:
                            if samples() >= before + 4:
                                return
                            time.sleep(.025)
                        self.fail(f'Rendering stalled during {label}:\n{output()}')

                    def cover(parent, w, h):
                        frame = parent.create_window(0, 0, w, h, 0, screen.root_depth, X.InputOutput,
                                                     X.CopyFromParent, override_redirect=True)
                        frame.map()
                        private.sync()
                        geometry = frame.get_geometry()
                        self.assertEqual((geometry.x, geometry.y, geometry.width, geometry.height), (0, 0, w, h))
                        self.assertEqual(frame.get_attributes().map_state, X.IsViewable)
                        return frame

                    if desktop_mode:
                        progress('drawing the root wallpaper')
                        self.assertIn(f'Found requested screen: {output_name}', output())
                    else:
                        progress('the renderer’s own full-size window')
                        pid_atom = private.intern_atom('_NET_WM_PID')
                        owned = []
                        for window in root.query_tree().children:
                            pid = window.get_full_property(pid_atom, Xatom.CARDINAL)
                            if pid and pid.value[0] == engine.pid and window.get_wm_name() == 'wallpaperengine':
                                owned.append(window)
                        self.assertEqual(len(owned), 1, output())
                        target = owned[0]
                        geometry = target.get_geometry()
                        self.assertEqual((geometry.width, geometry.height), (width, height))

                        if pause:
                            # Reparent only the app window created on this private display.
                            parent = cover(root, width, height)
                            target.reparent(parent, 0, 0)
                            target.map()
                            private.sync()
                            progress('the renderer’s reparenting frame')

                    if pause:
                        desktop = cover(root, width, height)
                        desktop.change_property(private.intern_atom('_NET_WM_WINDOW_TYPE'), Xatom.ATOM, 32,
                                                [private.intern_atom('_NET_WM_WINDOW_TYPE_DESKTOP')])
                        private.sync()
                        progress('a desktop-type window')
                        desktop_frame = cover(root, width, height)
                        desktop.reparent(desktop_frame, 0, 0)
                        desktop.map()
                        private.sync()
                        progress('a reparented desktop-type window')

                        normal = cover(root, width // 2, height // 2)
                        progress('a small window')
                        normal.destroy()

                    fullscreen = cover(root, width, height)
                    time.sleep(.16)
                    before = samples()
                    time.sleep(.48)
                    updates = samples() - before
                    self.assertIsNone(engine.poll(), output())
                    print('X11_PAUSE_RESULT ' + json.dumps({'pause': pause, 'desktop_mode': desktop_mode,
                                                           'covered_updates': updates, 'sample_seconds': .48}))
                    if pause:
                        self.assertEqual(updates, 0, output())
                    else:
                        self.assertGreaterEqual(updates, 4, output())
                    fullscreen.destroy()
                    private.sync()
                    progress('uncovering the output')
                finally:
                    if engine.poll() is None:
                        engine.send_signal(signal.SIGINT)
                    try:
                        engine.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        engine.kill()
                        engine.wait()
                self.assertEqual(engine.returncode, 0, path.read_text())


if __name__ == '__main__':
    unittest.main()
