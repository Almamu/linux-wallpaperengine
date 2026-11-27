"""Private-bus MPRIS regressions; no graphics or desktop media session required.

LWE_TEST_LIBRARY=build-release/output/liblinux-wallpaperengine-lib.so \
    python3 src/WallpaperEngine/Testing/test_media_source.py
Requires a C++ compiler, pkg-config, dbus-daemon, dbus-python and PyGObject.
The helper compiles the current media sources without rebuilding shared targets.
"""
import json
import os
from pathlib import Path
import queue
import signal
import subprocess
import sys
import tempfile
import threading
import time
import unittest


PROBE = r'''
#include <memory>
#include <optional>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <thread>
#include "WallpaperEngine/Media/DBusMediaSource.h"
using namespace WallpaperEngine::Media;
using namespace std::chrono_literals;

class PollProbe final : public MediaSource {
public:
    using MediaSource::MediaSource;
    int polls = 0;
    void expireInterval() { m_nextUpdate = {}; }

protected:
    void performUpdate() override {
        ++polls;
        fireMetadataListeners();
    }
};

int testPollReentry() {
    PollProbe source(1h);
    source.addMetadataListener([&](const auto&) {
        if (source.polls == 1) source.update();
    });
    source.update();
    return source.polls == 1 ? 0 : 1;
}

int testPollException() {
    PollProbe source(1h);
    auto removeListener = source.addMetadataListener([](const auto&) {
        throw std::runtime_error("poll failed");
    });
    try {
        source.update();
        return 1;
    } catch (const std::runtime_error&) {
    }
    removeListener();
    source.update();
    if (source.polls != 1) return 2;
    source.expireInterval();
    source.update();
    return source.polls == 2 ? 0 : 3;
}

int main(int argc, char* argv[]) {
    const std::string_view mode = argc > 1 ? argv[1] : "";
    if (mode == "poll-reentry") return testPollReentry();
    if (mode == "poll-exception") return testPollException();
    const auto source = DBusMediaSource::createOrFallback(20ms);
    auto& media = *source;
    if (mode == "idle") {
        int notices = 0;
        auto removeMetadata = media.addMetadataListener([&](const auto&) { ++notices; });
        auto removeArt = media.addAlbumArtListener([&](const auto&) { ++notices; });
        media.update();
        removeMetadata();
        removeArt();
        const auto& info = media.getMediaInfo();
        return !info.available && info.playbackState == MediaSource::Stopped && notices == 0 ? 0 : 1;
    }
    for (;;) {
        media.update();
        const auto& info = media.getMediaInfo();
        std::cout << "{\"title\":" << std::quoted(info.title)
                  << ",\"artist\":" << std::quoted(info.artist)
                  << ",\"album\":" << std::quoted(info.album)
                  << ",\"art\":" << std::quoted(info.url.value_or(""))
                  << ",\"state\":" << info.playbackState
                  << ",\"position\":" << info.position
                  << ",\"duration\":" << info.duration
                  << ",\"available\":" << (info.available ? "true" : "false")
                  << "}" << std::endl;
        std::this_thread::sleep_for(10ms);
    }
}
'''

PLAYER = r'''
import dbus, dbus.service, dbus.mainloop.glib, json, sys
from gi.repository import GLib
dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
class Player(dbus.service.Object):
    def __init__(self):
        self.name = dbus.service.BusName('org.mpris.MediaPlayer2.' + sys.argv[1], dbus.SessionBus())
        super().__init__(self.name, '/org/mpris/MediaPlayer2')
        self.state = sys.argv[2]
        self.metadata = {'xesam:title': sys.argv[1], 'xesam:artist': ['Original Artist'],
                         'xesam:album': 'Original Album', 'mpris:artUrl': 'file:///original.png',
                         'mpris:length': 180000000}
    def typedMetadata(self):
        result = {}
        for key, value in self.metadata.items():
            result[key] = dbus.Int64(value) if isinstance(value, int) else (
                dbus.Array(value, signature='s') if isinstance(value, list) else value)
        return dbus.Dictionary(result, signature='sv')
    @dbus.service.method('org.freedesktop.DBus.Properties', in_signature='ss', out_signature='v')
    def Get(self, iface, prop):
        if prop == 'PlaybackStatus': return self.state
        if prop == 'Position': return dbus.Int64(30000000)
        if prop == 'Metadata': return self.typedMetadata()
        raise dbus.exceptions.DBusException('Unknown property')
    @dbus.service.signal('org.freedesktop.DBus.Properties', signature='sa{sv}as')
    def PropertiesChanged(self, iface, props, invalidated): pass
    def command(self, source, condition):
        line = sys.stdin.readline()
        if not line: return False
        old_state = self.state
        self.state, self.metadata = json.loads(line)
        changed = {'Metadata': self.typedMetadata()}
        if self.state != old_state:
            changed['PlaybackStatus'] = self.state
        self.PropertiesChanged('org.mpris.MediaPlayer2.Player', changed, [])
        print('READY', flush=True)
        return True
player = Player()
GLib.io_add_watch(sys.stdin, GLib.IO_IN, player.command)
print('READY', flush=True)
GLib.MainLoop().run()
'''


@unittest.skipUnless(os.environ.get('LWE_TEST_LIBRARY'), 'Set LWE_TEST_LIBRARY for private DBus integration tests')
class MediaSource(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory(prefix='lwe-media-test-build-')
        cls.addClassCleanup(cls.build.cleanup)
        root = Path(__file__).resolve().parents[3]
        library = Path(os.environ['LWE_TEST_LIBRARY']).resolve()
        source = Path(cls.build.name) / 'probe.cpp'
        source.write_text(PROBE)
        cls.binary = Path(cls.build.name) / 'probe'
        flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'dbus-1'], text=True).split()
        subprocess.run([
            'c++', '-std=c++20', '-DNDEBUG',
            '-I', str(root / 'src'), '-I', str(root / 'src/External/json/include'),
            str(source), str(root / 'src/WallpaperEngine/Media/DBusMediaSource.cpp'),
            str(root / 'src/WallpaperEngine/Media/MediaSource.cpp'),
            str(library), '-Wl,-rpath,' + str(library.parent), *flags, '-o', str(cls.binary),
        ], check=True, capture_output=True, text=True)

    def setUp(self):
        self.children = []
        self.addCleanup(self.cleanup_children)
        daemon = self.child(['dbus-daemon', '--session', '--nofork', '--print-address=1'])
        address = daemon.stdout.readline().strip()
        self.assertTrue(address.startswith('unix:'), daemon.stderr.read() if not address else address)
        self.env = dict(os.environ, DBUS_SESSION_BUS_ADDRESS=address)

    def child(self, command, **kwargs):
        process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, text=True, **kwargs)
        self.children.append(process)
        return process

    def player(self, name, state):
        player = self.child([sys.executable, '-u', '-c', PLAYER, name, state], env=self.env)
        self.assertEqual(player.stdout.readline().strip(), 'READY')
        return player

    def command(self, player, state, metadata):
        player.stdin.write(json.dumps([state, metadata]) + '\n')
        player.stdin.flush()
        self.assertEqual(player.stdout.readline().strip(), 'READY')

    def start_probe(self):
        self.samples = queue.Queue()
        self.probe = self.child([str(self.binary)], env=self.env)
        def collect():
            for line in self.probe.stdout:
                if line.startswith('{'):
                    self.samples.put(json.loads(line))
        self.reader = threading.Thread(target=collect, daemon=True)
        self.reader.start()

    def wait_for(self, predicate, timeout=3):
        deadline = time.monotonic() + timeout
        last = None
        while time.monotonic() < deadline:
            try:
                last = self.samples.get(timeout=max(.01, deadline - time.monotonic()))
            except queue.Empty:
                break
            if predicate(last):
                return last
        self.fail(f'Media condition not reached; last snapshot: {last}')

    def test_missing_session_bus_keeps_media_idle(self):
        env = dict(self.env, DBUS_SESSION_BUS_ADDRESS=f'unix:path={self.build.name}/missing-bus')
        result = subprocess.run([self.binary, 'idle'], env=env, capture_output=True, text=True, timeout=3)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_poll_interval_survives_exception_and_reentry(self):
        for mode in ('poll-reentry', 'poll-exception'):
            with self.subTest(mode=mode):
                result = subprocess.run([self.binary, mode], capture_output=True, text=True, timeout=3)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_active_player_selection_ignores_foreign_signals_and_tracks_owner_lifecycle(self):
        paused = self.player('lwe_test_paused', 'Paused')
        active = self.player('lwe_test_active', 'Playing')
        self.start_probe()
        self.wait_for(lambda row: row['title'] == 'lwe_test_active' and row['state'] == 1)
        self.command(paused, 'Paused', {'xesam:title': 'Foreign Paused Track'})
        # Inspect every delivered sample, not just an eventual correction by polling.
        deadline = time.monotonic() + .25
        while time.monotonic() < deadline:
            row = self.samples.get(timeout=1)
            self.assertEqual(row['title'], 'lwe_test_active')
            self.assertEqual(row['state'], 1)
        self.stop(active)
        self.wait_for(lambda row: row['title'] == 'Foreign Paused Track' and row['state'] == 2)
        self.stop(paused)
        cleared = self.wait_for(lambda row: not row['available'])
        self.assertEqual(cleared, {'title': '', 'artist': '', 'album': '', 'art': '', 'state': 0,
                                   'position': 0, 'duration': 0, 'available': False})
        self.player('lwe_test_late', 'Playing')
        self.wait_for(lambda row: row['title'] == 'lwe_test_late' and row['state'] == 1)

    def test_metadata_dictionary_replaces_missing_and_empty_fields(self):
        player = self.player('lwe_test_track', 'Playing')
        self.start_probe()
        self.wait_for(lambda row: row['artist'] == 'Original Artist' and row['duration'] == 180000000)
        self.command(player, 'Playing', {'xesam:title': 'Title Only'})
        row = self.wait_for(lambda row: row['title'] == 'Title Only')
        self.assertEqual((row['artist'], row['album'], row['art'], row['duration']), ('', '', '', 0))
        self.command(player, 'Playing', {'xesam:title': 'Empty Fields', 'xesam:artist': [], 'mpris:artUrl': ''})
        row = self.wait_for(lambda row: row['title'] == 'Empty Fields')
        self.assertEqual((row['artist'], row['art']), ('', ''))
        self.command(player, 'Playing', {})
        row = self.wait_for(lambda row: row['title'] == '')
        self.assertEqual((row['artist'], row['album'], row['art'], row['duration']), ('', '', '', 0))

    def test_empty_idle_players_do_not_activate_media_integration(self):
        player = self.player('lwe_test_idle', 'Paused')
        self.command(player, 'Paused', {})
        self.start_probe()
        self.wait_for(lambda row: not row['available'] and row['state'] == 0)

        # Loading/clearing a paused track can emit Metadata without PlaybackStatus.
        self.command(player, 'Paused', {'xesam:title': 'Paused Track'})
        self.wait_for(lambda row: row['available'] and row['title'] == 'Paused Track' and row['state'] == 2)
        self.command(player, 'Paused', {})
        cleared = self.wait_for(lambda row: not row['available'])
        self.assertEqual((cleared['title'], cleared['state'], cleared['position']), ('', 0, 0))

        # Playing streams may legitimately have no descriptive metadata.
        self.command(player, 'Playing', {})
        self.wait_for(lambda row: row['available'] and row['state'] == 1)
        self.command(player, 'Stopped', {})
        self.wait_for(lambda row: not row['available'] and row['state'] == 0)

    @staticmethod
    def stop(process):
        if process.poll() is None:
            process.send_signal(signal.SIGINT)
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()

    def cleanup_children(self):
        for process in reversed(self.children):
            self.stop(process)
        if hasattr(self, 'reader'):
            self.reader.join(timeout=1)
        for process in self.children:
            for pipe in (process.stdin, process.stdout, process.stderr):
                pipe.close()


if __name__ == '__main__':
    unittest.main()
