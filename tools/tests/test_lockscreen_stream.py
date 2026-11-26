"""Compile the stream protocol helpers and inspect exported BMPs with an independent decoder."""
from pathlib import Path
import os
import json
import signal
import socket
import time
import stat
import subprocess
import tempfile
import unittest


PROBE = r'''
#include "WallpaperEngine/Application/LockscreenStream.h"
#include <cassert>
#include <chrono>
#include <vector>
using namespace WallpaperEngine::Application;
using namespace std::chrono_literals;
int main(int argc, char** argv) {
    const auto now=LockscreenStream::Clock::time_point(10s);
    LockscreenStream lease;
    assert(lease.fps(now)==0);
    assert(lease.renew("30", now));
    assert(lease.fps(now+2999ms)==30);
    assert(lease.fps(now+3s)==0);
    assert(lease.renew("60", now+2s));
    assert(lease.fps(now+4s)==60);
    for (const auto invalid : {"", "-1", "61", "30x", "30 20", "9999999999999"})
        assert(!lease.renew(invalid, now+3s));
    assert(lease.fps(now+4s)==60); // Bad requests do not revoke an existing lease.
    assert(lease.renew("0", now+4s));
    assert(lease.fps(now+4s)==0);
    assert((LockscreenStream::frameSize(2560,1440)==std::pair{960,540}));
    assert((LockscreenStream::frameSize(1080,1920)==std::pair{960,1707}));
    assert((LockscreenStream::frameSize(320,180)==std::pair{320,180}));
    assert((LockscreenStream::frameSize(0,180)==std::pair{0,0}));
    // Two bottom-up BGRA rows: blue/white at bottom, red/green at top.
    std::vector<unsigned char> pixels={255,0,0,255, 255,255,255,255,
                                       0,0,255,255, 0,255,0,255};
    assert(LockscreenStream::writeFrame(argv[1], "DP-1", 2,2,pixels));
    assert(!LockscreenStream::writeFrame(argv[1], "../escape", 2,2,pixels));
    assert(!LockscreenStream::writeFrame(argv[1], "HDMI-A-1", 4,2,pixels));
    const std::string path=std::string(argv[1])+"/DP-1.bmp";
    const int oldFrame=open(path.c_str(),O_RDONLY);
    assert(oldFrame>=0);
    pixels[0]=17;
    assert(LockscreenStream::writeFrame(argv[1], "DP-1", 2,2,pixels));
    unsigned char oldBlue=0;
    assert(pread(oldFrame,&oldBlue,1,54)==1 && oldBlue==255);
    close(oldFrame); // Replacement cannot modify an image a reader already opened.
    const std::string link=std::string(argv[1])+"-link";
    assert(symlink(argv[1],link.c_str())==0);
    assert(!LockscreenStream::writeFrame(link,"DP-1",2,2,pixels));
    const std::string unsafe=std::string(argv[1])+"-unsafe";
    assert(mkdir(unsafe.c_str(),0755)==0);
    assert(!LockscreenStream::writeFrame(unsafe,"DP-1",2,2,pixels));
    // The X11 root pixmap puts the first readback row at the top of the display.
    assert(LockscreenStream::writeFrame(argv[1], "X11-1", 2,2,pixels, /*bottomUp=*/false));
}
'''


class LockscreenStreamProtocol(unittest.TestCase):
    def test_lease_geometry_and_atomic_private_bmp(self):
        from PIL import Image
        root = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory(prefix='lwe-lock-stream-') as directory:
            folder = Path(directory)
            source = folder / 'probe.cpp'
            source.write_text(PROBE)
            binary = folder / 'probe'
            subprocess.run(['c++', '-std=c++20', '-O2', '-I', str(root / 'src'),
                            str(source), '-o', str(binary)], check=True)
            subprocess.run([binary, folder / 'frames'], check=True)
            self.assertEqual(stat.S_IMODE((folder / 'frames').stat().st_mode), 0o700)
            frame = folder / 'frames/DP-1.bmp'
            self.assertEqual(stat.S_IMODE(frame.stat().st_mode), 0o600)
            x11_frame = folder / 'frames/X11-1.bmp'
            self.assertCountEqual((folder / 'frames').iterdir(), [frame, x11_frame])
            self.assertFalse((folder / 'escape.bmp').exists())
            image = Image.open(frame).convert('RGB')
            self.assertEqual(image.size, (2, 2))
            self.assertEqual([image.getpixel((x, y)) for y in range(2) for x in range(2)], [(255, 0, 0), (0, 255, 0),
                                                     (0, 0, 17), (255, 255, 255)])
            x11_image = Image.open(x11_frame).convert('RGB')
            self.assertEqual([x11_image.getpixel((x, y)) for y in range(2) for x in range(2)],
                             [(0, 0, 17), (255, 255, 255), (255, 0, 0), (0, 255, 0)])


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for isolated graphics tests')
class LockscreenStreamGraphics(unittest.TestCase):
    def test_current_crop_orientation_motion_and_lease_expiry(self):
        from PIL import Image
        with tempfile.TemporaryDirectory(prefix='lwe-lock-gl-') as directory:
            root = Path(directory)
            runtime = root / 'runtime'
            runtime.mkdir(mode=0o700)
            scene = {
                'camera': {'eye': '0 0 1', 'center': '0 0 0', 'up': '0 1 0'},
                'general': {'orthogonalprojection': {'width': 320, 'height': 320},
                            'clearcolor': '0 0 0', 'bloom': False, 'camerafade': False},
                'objects': [
                    {'id': 1, 'image': 'models/util/solidlayer.json', 'size': '160 160',
                     'origin': '80 240 0', 'color': '1 0 0'},
                    {'id': 2, 'image': 'models/util/solidlayer.json', 'size': '160 160',
                     'origin': '240 240 0', 'color': '0 1 0'},
                    {'id': 3, 'image': 'models/util/solidlayer.json', 'size': '160 160',
                     'origin': '80 80 0', 'color': '0 0 1'},
                    {'id': 4, 'image': 'models/util/solidlayer.json', 'size': '160 160',
                     'origin': '240 80 0', 'color': '1 1 1'},
                    {'id': 5, 'image': 'models/util/solidlayer.json', 'size': '20 20',
                     'origin': {'value': '160 160 0', 'script':
                        'export function update() { return new Vec3(160 + 50*Math.sin(engine.runtime*4),160,0); }'},
                     'color': '1 0 1'},
                    # Fill crops away these source-canvas edges in the 16:9 output.
                    {'id': 6, 'image': 'models/util/solidlayer.json', 'size': '320 20',
                     'origin': '160 310 0', 'color': '1 0 1'},
                    {'id': 7, 'image': 'models/util/solidlayer.json', 'size': '320 20',
                     'origin': '160 10 0', 'color': '1 0 1'}]}
            for obj in scene['objects']:
                obj['name'] = f"Probe {obj['id']}"
            (root / 'project.json').write_text(json.dumps({'type': 'scene', 'file': 'scene.json', 'title': 'Lock feed regression'}))
            (root / 'scene.json').write_text(json.dumps(scene))
            binary = Path(os.environ['LWE_TEST_BINARY']).resolve()
            ipc = root / 'control.sock'
            env = dict(os.environ, LD_LIBRARY_PATH=str(binary.parent), WPE_CONTROL_SOCKET=str(ipc),
                       WPE_LOG_FILE='off', XDG_RUNTIME_DIR=str(runtime), XDG_SESSION_TYPE='x11',
                       SDL_AUDIODRIVER='dummy')
            # This opt-in test runs inside the caller's isolated Gamescope/X11 session.
            env.pop('WAYLAND_DISPLAY', None)
            def command(value):
                with socket.socket(socket.AF_UNIX) as client:
                    client.settimeout(5)
                    client.connect(str(ipc))
                    client.sendall((value + '\n').encode())
                    return client.recv(4096).decode()
            with (root / 'engine.log').open('w') as log:
                process = subprocess.Popen([str(binary), '--window', '0x0x1280x720', '--silent',
                    '--fps', '60', '--scaling', 'fill', '--no-full-screen-pause', '--disable-mouse', '--disable-parallax',
                    '--no-audio-processing', '--noautomute', str(root)], env=env, stdout=log, stderr=subprocess.STDOUT)
                try:
                    deadline = time.monotonic() + 30
                    while not ipc.exists() and process.poll() is None and time.monotonic() < deadline:
                        time.sleep(.05)
                    self.assertTrue(ipc.exists(), (root / 'engine.log').read_text())
                    self.assertTrue(command('lockscreen-stream 30').startswith('ok'))
                    frame = runtime / 'linux-wallpaperengine-lock/default.bmp'
                    deadline = time.monotonic() + 10
                    while not frame.exists() and process.poll() is None and time.monotonic() < deadline:
                        time.sleep(.05)
                    self.assertTrue(frame.exists(), (root / 'engine.log').read_text())
                    image = Image.open(frame).convert('RGB')
                    self.assertEqual(image.size, (960, 540))
                    for position, expected in [((120, 9), (255, 0, 0)), ((840, 90), (0, 255, 0)),
                                                ((120, 531), (0, 0, 255)), ((840, 450), (255, 255, 255))]:
                        self.assertEqual(image.getpixel(position), expected)
                    first = frame.read_bytes()
                    time.sleep(.35)
                    self.assertNotEqual(frame.read_bytes(), first)
                    self.assertTrue(command('lockscreen-stream 0').startswith('ok'))
                    stopped = frame.stat().st_mtime_ns
                    time.sleep(.2)
                    self.assertEqual(frame.stat().st_mtime_ns, stopped)
                    self.assertTrue(command('lockscreen-stream 30').startswith('ok'))
                    time.sleep(3.2)
                    expired = frame.stat().st_mtime_ns
                    time.sleep(.2)
                    self.assertEqual(frame.stat().st_mtime_ns, expired)
                finally:
                    process.send_signal(signal.SIGINT)
                    try:
                        process.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()
                self.assertEqual(process.returncode, 0, (root / 'engine.log').read_text())
                self.assertNotIn('OpenGL error:', (root / 'engine.log').read_text())


if __name__ == '__main__':
    unittest.main()
