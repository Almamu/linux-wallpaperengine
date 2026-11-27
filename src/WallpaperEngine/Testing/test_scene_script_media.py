"""SceneScript media callbacks on a private MPRIS bus.

Run on a private graphics display with LWE_TEST_BINARY set to the renderer.
LWE_TEST_ASSETS optionally selects Wallpaper Engine's assets directory.
"""
import json
import os
from pathlib import Path
import select
import shutil
import subprocess
import sys
import tempfile
import time
import unittest

import test_media_source


MARKER = 'LWE_SCENE_MEDIA '
SCRIPT = '''
let ready = false;
function emit(kind, value) {
    console.log('LWE_SCENE_MEDIA ' + JSON.stringify([kind, value]));
}
export function update(value) {
    if (!ready && engine.runtime >= 0.35) {
        ready = true;
        emit('ready', null);
    }
    return value;
}
export function mediaPropertiesChanged(event) {
    emit('properties', [event.title, event.artist, event.albumTitle]);
}
export function mediaPlaybackChanged(event) { emit('playback', event.state); }
export function mediaTimelineChanged(event) { emit('timeline', [event.position, event.duration]); }
export function mediaThumbnailChanged(event) { emit('thumbnail', event.hasThumbnail); }
'''


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class SceneScriptMedia(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='lwe-scene-media-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.engine_log = self.root/'engine.log'
        if artifacts := os.environ.get('LWE_TEST_ARTIFACTS'):
            self.addCleanup(shutil.copytree, self.root, Path(artifacts)/self._testMethodName,
                            dirs_exist_ok=True, ignore=shutil.ignore_patterns('*.sock'))
        self.env = dict(os.environ, WPE_CONTROL_SOCKET=str(self.root/'control.sock'), WPE_LOG_FILE='off')
        socket = 'unix:path='+str(self.root/'session.sock')
        daemon = self.child(['dbus-daemon', '--session', '--nofork', '--print-address=1', '--address='+socket])
        address = self.readline(daemon)
        self.assertTrue(address.startswith(socket), address)
        self.env['DBUS_SESSION_BUS_ADDRESS'] = address

    def child(self, command, stdout=subprocess.PIPE):
        process = subprocess.Popen(command, env=self.env, stdin=subprocess.PIPE, stdout=stdout,
                                   stderr=subprocess.STDOUT, text=True)
        self.addCleanup(self.stop, process)
        return process

    @staticmethod
    def stop(process):
        test_media_source.MediaSource.stop(process)
        process.stdin.close()
        if process.stdout is not None:
            process.stdout.close()

    def readline(self, process):
        self.assertTrue(select.select([process.stdout], [], [], 8)[0], 'Missing process readiness marker')
        return process.stdout.readline().strip()

    def player(self, name):
        player = self.child([sys.executable, '-u', '-c', test_media_source.PLAYER, name, 'Playing'])
        self.assertEqual(self.readline(player), 'READY')
        return player

    def update_player(self, player, state, metadata):
        player.stdin.write(json.dumps([state, metadata])+'\n')
        player.stdin.flush()
        self.assertEqual(self.readline(player), 'READY')

    def start_scene(self):
        scene = {'camera': {'eye': '0 0 1', 'center': '0 0 0', 'up': '0 1 0'},
                 'general': {'orthogonalprojection': {'width': 320, 'height': 180}, 'clearcolor': '0 0 0'},
                 'objects': [{'id': 1, 'name': 'media probe', 'solid': True,
                              'visible': {'value': True, 'script': SCRIPT}}]}
        (self.root/'scene.json').write_text(json.dumps(scene))
        (self.root/'project.json').write_text(json.dumps({'type': 'scene', 'file': 'scene.json', 'title': 'SceneScript media probe'}))
        binary = Path(os.environ['LWE_TEST_BINARY']).resolve()
        command = [str(binary), '--window', '0x0x320x180', '--fps', '24', '--silent', '--no-audio-processing',
                   '--no-fullscreen-pause', '--noautomute', '--disable-persistent-cache', str(self.root)]
        if assets := os.environ.get('LWE_TEST_ASSETS'):
            command.extend(['--assets-dir', assets])
        with self.engine_log.open('w') as output:
            self.renderer = self.child(command, stdout=output)
        return self.expect(['ready', None])

    def expect(self, *expected, after=0):
        deadline = time.monotonic()+12
        while time.monotonic() < deadline and self.renderer.poll() is None:
            output = self.engine_log.read_text()
            events = [json.loads(line.split(MARKER, 1)[1]) for line in output.splitlines(keepends=True)
                      if MARKER in line and line.endswith(']\n')]
            if all(event in events[after:] for event in expected):
                return events
            time.sleep(.025)
        self.fail(f'Missing SceneScript events {expected}:\n{self.engine_log.read_text()}')

    def expect_track(self, title, after=0):
        return self.expect(['properties', [title, 'Original Artist', 'Original Album']], ['playback', 1],
                           ['timeline', [30, 180]], ['thumbnail', True], after=after)

    def test_startup_with_available_player(self):
        self.player('lwe_at_startup')
        self.start_scene()
        self.expect_track('lwe_at_startup')

    def test_empty_startup_and_later_player_lifecycle(self):
        events = self.start_scene()
        with self.subTest(phase='no provider'):
            self.assertEqual(events, [['ready', None]])
        player = self.player('lwe_arrives_later')
        events = self.expect_track('lwe_arrives_later', after=len(events))
        metadata = {'xesam:title': 'Changed title', 'mpris:length': 240_000_000}
        self.update_player(player, 'Paused', metadata)
        events = self.expect(['properties', ['Changed title', '', '']], ['playback', 2],
                             ['timeline', [30, 240]], ['thumbnail', False], after=len(events))
        self.update_player(player, 'Paused', {**metadata, 'mpris:artUrl': 'file:///returning-art.png'})
        events = self.expect(['thumbnail', True], after=len(events))
        self.stop(player)
        events = self.expect(['properties', ['', '', '']], ['playback', 0], ['timeline', [0, 0]],
                             ['thumbnail', False], after=len(events))
        self.player('lwe_arrives_later')
        self.expect_track('lwe_arrives_later', after=len(events))

    def test_no_session_bus_starts_silently(self):
        self.env['DBUS_SESSION_BUS_ADDRESS'] = 'unix:path='+str(self.root/'missing.sock')
        self.assertEqual(self.start_scene(), [['ready', None]])


if __name__ == '__main__':
    unittest.main()
