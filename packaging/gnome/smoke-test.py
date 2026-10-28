#!/usr/bin/env python3
"""Exercise the extension in a disposable GNOME 42 Wayland session."""

import argparse
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time


SOURCE = Path(__file__).resolve().parent

PROBE_UUID = 'wallpaper-test-probe@local.test'
# Installed only inside the disposable session. This exposes no unlock method.
TEST_PROBE = """
const {Gio} = imports.gi;
const Main = imports.ui.main;
const XML = `<node><interface name="org.gnome.WallpaperTestProbe">
<method name="Wake"/><method name="ShowPrompt"/><method name="Lock"/><method name="RestackStatic"/>
<method name="GetState"><arg type="s" direction="out"/></method>
</interface></node>`;
class Probe {
    enable() {
        const shield = Main.screenShield;
        const original = shield._setLocked;
        this._originalSetLocked = original;
        this._setLocked = function (locked) {
            const session = this._loginSession;
            this._loginSession = null;
            try { original.call(this, locked); }
            finally { this._loginSession = session; }
        };
        shield._setLocked = this._setLocked;
        this._wakeCount = 0;
        this._wakeId = Main.screenShield.connect('wake-up-screen', () => this._wakeCount++);
        this._object = Gio.DBusExportedObject.wrapJSObject(XML, this);
        this._object.export(Gio.DBus.session, '/org/gnome/WallpaperTestProbe');
        this._owner = Gio.bus_own_name_on_connection(Gio.DBus.session,
            'org.gnome.WallpaperTestProbe', Gio.BusNameOwnerFlags.NONE, null, null);
    }
    RestackStatic() {
        // Model a secondary static background restacked during a display transition.
        const index = Main.layoutManager.monitors.findIndex(m => m.index !== Main.layoutManager.primaryIndex);
        const actor = Main.layoutManager._bgManagers[index].backgroundActor;
        Main.layoutManager._backgroundGroup.set_child_above_sibling(actor, null);
    }
    Lock() { Main.screenShield.lock(true); }
    Wake() {
        Main.screenShield._longLightbox.lightOff();
        Main.screenShield._shortLightbox.lightOff();
    }
    ShowPrompt() { Main.screenShield._dialog._showPrompt(); }
    GetState() {
        const dialog = Main.screenShield._dialog;
        return JSON.stringify({
            mode: Main.sessionMode.currentMode,
            locked: Main.screenShield.locked,
            blank: Main.screenShield._shortLightbox.active || Main.screenShield._longLightbox.active,
            wakeCount: this._wakeCount,
            blur: dialog ? dialog._backgroundGroup.get_children().map(widget => {
                const effect = widget.get_effect('blur');
                return effect ? effect.enabled : false;
            }) : [],
            desktop: Main.layoutManager._backgroundGroup.get_children().map(actor => ({
                type: actor.constructor.name, visible: actor.visible,
                x: actor.x, y: actor.y, width: actor.width, height: actor.height,
            })),
            backgrounds: dialog ? dialog._backgroundGroup.get_n_children() : 0,
            promptVisible: dialog ? dialog._promptBox.visible : false,
        });
    }
    disable() {
        if (Main.screenShield._setLocked === this._setLocked)
            Main.screenShield._setLocked = this._originalSetLocked;
        Main.screenShield.disconnect(this._wakeId);
        this._object.unexport();
        Gio.bus_unown_name(this._owner);
    }
}
function init() { return new Probe(); }
"""


def wait_for(predicate):
    """Poll until the predicate succeeds, or fail after sixty seconds."""
    deadline = time.monotonic() + 60
    while time.monotonic() < deadline:
        result = predicate()
        if result:
            return result
        time.sleep(0.1)
    raise AssertionError('Timed out waiting for a GNOME/renderer state transition')


def run_test(args):
    """Exercise rendering and lifecycle behavior on the private session bus."""
    import gi
    gi.require_version('Gio', '2.0')
    from gi.repository import Gio, GLib
    from PIL import Image, ImageChops, ImageStat

    extension_dir = Path(os.environ['XDG_DATA_HOME']) / 'gnome-shell/extensions' / args.uuid

    def setting(key, value):
        """Write an extension setting into the isolated keyfile backend."""
        subprocess.run(['gsettings', '--schemadir', str(extension_dir / 'schemas'),
                        'set', args.schema, key, value], check=True)

    def extension(method):
        """Call the Shell extension API for the extension under test."""
        return subprocess.check_output([
            'gdbus', 'call', '--session', '--dest', 'org.gnome.Shell',
            '--object-path', '/org/gnome/Shell',
            '--method', 'org.gnome.Shell.Extensions.' + method, args.uuid], text=True)

    def renderer_for(project):
        """Find an owned project renderer, failing immediately if Shell exits."""
        returncode = shell.poll()
        if returncode is not None:
            raise AssertionError('gnome-shell exited with code %s' % returncode)
        children = Path('/proc/%d/task/%d/children' % (shell.pid, shell.pid))
        if not children.exists():
            return None
        for child in children.read_text().split():
            try:
                argv = Path('/proc/%s/cmdline' % child).read_bytes().rstrip(b'\0').split(b'\0')
            except FileNotFoundError:
                continue
            if argv[-1] == str(project).encode():
                return int(child)
        return None

    def renderer():
        """Find the desktop renderer among the disposable compositor children."""
        return renderer_for(args.wallpaper)

    def usage(pid):
        # Fields after comm start with state (field 3); comm can contain spaces.
        """Return the process state and cumulative CPU ticks from procfs."""
        fields = Path('/proc/%d/stat' % pid).read_text().rsplit(')', 1)[1].split()
        return fields[0], int(fields[11]) + int(fields[12])

    def wait_until_paused(pid, paused):
        """Wait until the renderer reaches the requested stopped state."""
        wait_for(lambda: (usage(pid)[0] == 'T') == paused)

    def wait_until_exited(pid):
        """Wait until the renderer has exited and been reaped."""
        wait_for(lambda: not Path('/proc/%d' % pid).exists())

    def screenshot(name):
        """Capture the private compositor and assert that capture succeeded."""
        path = args.output / (name + '.png')
        reply = connection.call_sync(
            'org.gnome.Shell.Screenshot', '/org/gnome/Shell/Screenshot',
            'org.gnome.Shell.Screenshot', 'Screenshot',
            GLib.Variant('(bbs)', (False, False, str(path))),
            None, Gio.DBusCallFlags.NONE, 15000, None)
        assert reply.unpack()[0]
        return Image.open(path).convert('RGB')

    setting('engine-path', json.dumps(str(args.engine)))
    setting('wallpaper', json.dumps(str(args.wallpaper)))
    setting('assets-dir', json.dumps(str(args.assets_dir)) if args.assets_dir else "''")
    subprocess.run(['gsettings', 'set', 'org.gnome.shell', 'enabled-extensions',
                    json.dumps([args.uuid, PROBE_UUID])], check=True)
    subprocess.run(['gsettings', 'set', 'org.gnome.shell', 'disabled-extensions', '[]'], check=True)

    with (args.output / 'gnome-shell.log').open('w') as log:
        shell = subprocess.Popen([
            'gnome-shell', '--headless', '--wayland', '--no-x11', '--wayland-display=lwe-smoke',
            *(['--virtual-monitor', '2560x1440'] * args.monitors)],
            stdout=log, stderr=subprocess.STDOUT)
        try:
            pid = wait_for(renderer)
            wait_for(lambda: 'VO: [libmpv]' in (args.output / 'gnome-shell.log').read_text())
            connection = Gio.bus_get_sync(Gio.BusType.SESSION, None)
            connection.call_sync(
                'org.gnome.Shell', '/org/gnome/Shell', 'org.freedesktop.DBus.Properties', 'Set',
                GLib.Variant('(ssv)', ('org.gnome.Shell', 'OverviewActive', GLib.Variant('b', False))),
                None, Gio.DBusCallFlags.NONE, 5000, None)
            if args.lock_lifecycle:
                set_test_primary(connection, args.monitors - 1)
                time.sleep(6)
                pid = wait_for(renderer)
            assert "'error': <''>" in extension('GetExtensionInfo')
            reply = connection.call_sync(
                'org.freedesktop.DBus', '/org/freedesktop/DBus', 'org.freedesktop.DBus',
                'RequestName', GLib.Variant('(su)', ('org.gnome.Screenshot', 0)),
                None, Gio.DBusCallFlags.NONE, 5000, None)
            assert reply.unpack()[0] == 1
            gi.require_version('Gtk', '3.0')
            gi.require_version('Gdk', '3.0')
            from gi.repository import Gdk, Gtk
            Gtk.init([])
            display = Gdk.Display.get_default()
            assert display.get_n_monitors() == args.monitors
            geometries = [display.get_monitor(index).get_geometry() for index in range(args.monitors)]

            def crop_monitor(image, index):
                """Exclude panel edges from the selected monitor image."""
                rect = geometries[index]
                return image.crop((rect.x + 200, rect.y + 200,
                                   rect.x + rect.width - 200, rect.y + rect.height - 200))

            frames = []
            for index in range(2):
                time.sleep(5)
                frames.append(screenshot('background-%d' % index))
            differences = []
            for index in range(args.monitors):
                cropped = [crop_monitor(frame, index) for frame in frames]
                assert max(ImageStat.Stat(cropped[0]).stddev) > 10
                difference = ImageStat.Stat(ImageChops.difference(*cropped)).mean
                assert max(difference) > 1, difference
                differences.append(difference)
            print('PASS: composited background frames change on every monitor', flush=True)

            if args.lock_lifecycle:
                test_lock_lifecycle(args, connection, setting, screenshot, crop_monitor)
            setting('paused', 'true')
            wait_for(lambda: usage(pid)[0] == 'T')
            ticks = usage(pid)[1]
            time.sleep(2)
            assert usage(pid)[1] == ticks
            setting('paused', 'false')
            wait_for(lambda: usage(pid)[0] != 'T' and usage(pid)[1] > ticks)
            print('PASS: pause stops CPU work and resume restarts it', flush=True)
            pid = test_renderer_crash(args.wallpaper, renderer_for, usage)
            first = screenshot('recovered-desktop-0')
            time.sleep(3)
            second = screenshot('recovered-desktop-1')
            for index in range(args.monitors):
                difference = ImageStat.Stat(ImageChops.difference(
                    crop_monitor(first, index), crop_monitor(second, index))).mean
                assert max(difference) > 1, difference
            print('PASS: desktop animation recovers after renderer crash', flush=True)

            fullscreen_code = """
import sys
import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk
window = Gtk.Window(title='Wallpaper fullscreen test')
window.fullscreen_on_monitor(window.get_screen(), int(sys.argv[1]))
window.show_all()
Gtk.main()
"""
            fullscreen = []
            try:
                fullscreen.append(subprocess.Popen([sys.executable, '-c', fullscreen_code, '0']))
                if args.monitors == 2:
                    time.sleep(2)
                    assert usage(pid)[0] != 'T'
                    first = crop_monitor(screenshot('one-fullscreen-0'), 1)
                    time.sleep(3)
                    second = crop_monitor(screenshot('one-fullscreen-1'), 1)
                    assert max(ImageStat.Stat(ImageChops.difference(first, second)).mean) > 1
                    print('PASS: one fullscreen monitor leaves the other wallpaper animated', flush=True)
                    fullscreen.append(subprocess.Popen([sys.executable, '-c', fullscreen_code, '1']))
                wait_until_paused(pid, True)
                if args.lock_wallpaper:
                    test_lock_screen(args, connection, setting, screenshot, crop_monitor,
                                     renderer_for, usage, extension, pid)
                    pid = wait_for(renderer)
                    wait_until_paused(pid, True)
                fullscreen[0].terminate()
                fullscreen[0].wait(timeout=5)
                wait_until_paused(pid, False)
            finally:
                for process in fullscreen:
                    if process.poll() is None:
                        process.terminate()
                        process.wait(timeout=5)
            wait_for(lambda: usage(pid)[0] != 'T')
            print('PASS: fullscreen pause and resume', flush=True)

            old_pid = pid
            setting('fps', '25')
            pid = wait_for(lambda: renderer() if renderer() != old_pid else None)
            wait_for(lambda: not Path('/proc/%d' % old_pid).exists())
            assert b'25' in Path('/proc/%d/cmdline' % pid).read_bytes().split(b'\0')
            print('PASS: settings change restarts the renderer with new arguments', flush=True)

            for paused in (False, True):
                setting('paused', 'true' if paused else 'false')
                wait_until_paused(pid, paused)
                assert 'true' in extension('DisableExtension')
                wait_until_exited(pid)
                setting('paused', 'false')
                assert 'true' in extension('EnableExtension')
                pid = wait_for(renderer)
            assert 'true' in extension('DisableExtension')
            wait_for(lambda: not Path('/proc/%d' % pid).exists())
            print('PASS: running/paused child cleanup and re-enable', flush=True)
            (args.output / 'result.json').write_text(json.dumps({
                'passed': True, 'monitor_count': args.monitors,
                'background_difference_rgb': differences,
                'lock_screen_tested': bool(args.lock_wallpaper),
                'environment': 'isolated headless GNOME 42; functional test only',
            }, indent=2) + '\n')
        finally:
            stop_test_session(shell, extension, renderer)


def set_test_primary(connection, index):
    """Select the right-hand primary output on the disposable compositor only."""
    from gi.repository import Gio, GLib
    destination = 'org.gnome.Mutter.DisplayConfig'
    path = '/org/gnome/Mutter/DisplayConfig'
    state = connection.call_sync(destination, path, destination, 'GetCurrentState',
                                 None, None, Gio.DBusCallFlags.NONE, 5000, None).unpack()
    modes = {spec[0]: next(mode[0] for mode in modes if mode[6].get('is-current'))
             for spec, modes, _properties in state[1]}
    monitors = [(x, y, scale, transform, monitor_index == index,
                 [(spec[0], modes[spec[0]], {}) for spec in specs])
                for monitor_index, (x, y, scale, transform, _primary, specs, _properties)
                in enumerate(state[2])]
    connection.call_sync(destination, path, destination, 'ApplyMonitorsConfig',
                         GLib.Variant('(uua(iiduba(ssa{sv}))a{sv})',
                                      (state[0], 1, monitors, {})),
                         None, Gio.DBusCallFlags.NONE, 5000, None)


def test_lock_lifecycle(args, connection, setting, screenshot, crop_monitor):
    """Check real locking, clear video, keep-awake signals and both outputs after unlock."""
    from gi.repository import Gio, GLib
    from PIL import ImageChops, ImageStat

    def probe(method):
        """Call the isolated probe without exporting controls into the desktop session."""
        return connection.call_sync('org.gnome.WallpaperTestProbe', '/org/gnome/WallpaperTestProbe',
                                    'org.gnome.WallpaperTestProbe', method, None, None,
                                    Gio.DBusCallFlags.NONE, 15000, None).unpack()

    setting('keep-lock-screen-on', 'true')
    setting('lock-wallpapers', '[]')
    setting('lock-mirrors', '[true, false]')
    failures = []
    for cycle in range(2):
        probe('Lock')
        time.sleep(7)
        state = json.loads(probe('GetState')[0])
        assert state['locked'], state
        if state['blank']:
            failures.append('Lock cycle %d faded to black' % cycle)
            # Diagnose the subsequent unlock even on the unfixed baseline.
            probe('Wake')
        if any(state['blur']):
            failures.append('Lock cycle %d blurred the video' % cycle)
        if cycle == 0:
            time.sleep(32)
            later = json.loads(probe('GetState')[0])
            if later['wakeCount'] - state['wakeCount'] < 3:
                failures.append('No periodic screen wake requests while locked')
        first = screenshot('lifecycle-lock-%d-0' % cycle)
        time.sleep(3)
        second = screenshot('lifecycle-lock-%d-1' % cycle)
        for index in range(args.monitors):
            change = ImageStat.Stat(ImageChops.difference(
                crop_monitor(first, index), crop_monitor(second, index))).mean
            if max(change) < 0.3:
                failures.append('Lock cycle %d monitor %d does not animate' % (cycle, index))
        probe('ShowPrompt')
        time.sleep(1)
        state = json.loads(probe('GetState')[0])
        assert state['promptVisible'], state
        if any(state['blur']):
            failures.append('Password prompt blurred the wallpaper')
        if cycle == 1 and args.monitors == 2:
            probe('RestackStatic')
        connection.call_sync('org.gnome.ScreenSaver', '/org/gnome/ScreenSaver',
                             'org.gnome.ScreenSaver', 'SetActive', GLib.Variant('(b)', (False,)),
                             None, Gio.DBusCallFlags.NONE, 5000, None)
        wait_for(lambda: not json.loads(probe('GetState')[0])['locked'])
        time.sleep(3)
        state = json.loads(probe('GetState')[0])
        first = screenshot('lifecycle-desktop-%d-0' % cycle)
        time.sleep(3)
        second = screenshot('lifecycle-desktop-%d-1' % cycle)
        for index in range(args.monitors):
            change = ImageStat.Stat(ImageChops.difference(
                crop_monitor(first, index), crop_monitor(second, index))).mean
            if max(change) < 1:
                failures.append('Unlock cycle %d monitor %d shows a static background' % (cycle, index))
        (args.output / ('lifecycle-state-%d.json' % cycle)).write_text(json.dumps(state, indent=2))
    state = json.loads(probe('GetState')[0])
    time.sleep(12)
    assert json.loads(probe('GetState')[0])['wakeCount'] == state['wakeCount'], 'Wake timer survived unlock'
    setting('keep-lock-screen-on', 'false')
    (args.output / 'lifecycle-result.json').write_text(json.dumps({'passed': not failures, 'failures': failures}, indent=2))
    assert not failures, failures
    print('PASS: real lock cycles retain clear video, wake requests and both desktop outputs', flush=True)


def test_renderer_crash(project, renderer_for, usage):
    """Kill an owned test renderer and verify delayed recovery to a new running child."""
    old_pid = wait_for(lambda: renderer_for(project))
    started = time.monotonic()
    os.kill(old_pid, signal.SIGKILL)
    wait_for(lambda: not Path('/proc/%d' % old_pid).exists())
    new_pid = wait_for(lambda: renderer_for(project))
    assert new_pid != old_pid
    assert time.monotonic() - started >= 3, 'Recovery must back off instead of looping immediately'
    wait_for(lambda: usage(new_pid)[0] != 'T')
    # A mapped window can precede video initialization; allow it to receive frames.
    time.sleep(3)
    return new_pid


def stop_test_session(shell, extension, renderer):
    """Disable playback and reap the disposable compositor on every exit path."""
    if shell.poll() is None:
        try:
            extension('DisableExtension')
        finally:
            # A renderer stuck during startup may not handle SIGTERM. Only
            # kill a child still owned by this test's compositor.
            deadline = time.monotonic() + 5
            while renderer() and time.monotonic() < deadline:
                time.sleep(0.1)
            child = renderer()
            if child:
                try:
                    os.kill(child, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            shell.terminate()
            try:
                shell.wait(timeout=10)
            except subprocess.TimeoutExpired:
                shell.kill()
                shell.wait()


def test_lock_screen(args, connection, setting, screenshot, crop_monitor,
                     renderer_for, usage, extension, desktop_pid):
    """Check independent lock backgrounds, pause, prompt visibility and cleanup."""
    from gi.repository import Gio, GLib
    from PIL import ImageChops, ImageStat

    def probe(method):
        """Call the test-only visual probe on the disposable session bus."""
        return connection.call_sync(
            'org.gnome.WallpaperTestProbe', '/org/gnome/WallpaperTestProbe',
            'org.gnome.WallpaperTestProbe', method, None, None,
            Gio.DBusCallFlags.NONE, 15000, None).unpack()

    def screen_saver(method, parameters=None):
        """Activate or dismiss the private screen shield without credentials."""
        return connection.call_sync(
            'org.gnome.ScreenSaver', '/org/gnome/ScreenSaver', 'org.gnome.ScreenSaver',
            method, parameters, None, Gio.DBusCallFlags.NONE, 15000, None).unpack()

    # SetActive exercises the real shield/dialog without requiring credentials
    # in the disposable compositor. Never issue these calls on the user's bus.
    wallpapers = [str(args.wallpaper)]
    if args.monitors == 2:
        wallpapers.append(str(args.lock_wallpaper))
    else:
        wallpapers[0] = str(args.lock_wallpaper)
    setting('lock-wallpapers', json.dumps(wallpapers))
    setting('lock-mirrors', '[false, true]' if args.monitors == 2 else '[false]')
    desktop_pid = wait_for(lambda: renderer_for(args.wallpaper))
    wait_for(lambda: usage(desktop_pid)[0] == 'T')
    screen_saver('SetActive', GLib.Variant('(b)', (True,)))
    wait_for(lambda: screen_saver('GetActive')[0])
    probe('Wake')
    state = json.loads(probe('GetState')[0])
    assert state['mode'] == 'unlock-dialog' and state['backgrounds'] == args.monitors, state
    lock_pid = wait_for(lambda: renderer_for(args.lock_wallpaper))
    wait_for(lambda: usage(lock_pid)[0] != 'T')
    assert "'error': <''>" in extension('GetExtensionInfo')
    frames = []
    for index in range(2):
        time.sleep(5)
        frames.append(screenshot('lock-%d' % index))
    differences = []
    for index in range(args.monitors):
        cropped = [crop_monitor(frame, index) for frame in frames]
        assert max(ImageStat.Stat(cropped[0]).stddev) > 5
        difference = ImageStat.Stat(ImageChops.difference(*cropped)).mean
        assert max(difference) > 0.3, difference
        differences.append(difference)
    if args.monitors == 2:
        left, right = [crop_monitor(frames[0], index) for index in range(2)]
        assert max(ImageStat.Stat(ImageChops.difference(left, right)).mean) > 3
        assert usage(desktop_pid)[0] != 'T', 'Lock playback must ignore desktop fullscreen windows'
    else:
        assert usage(desktop_pid)[0] == 'T', 'Unused desktop renderer must pause on lock'
    if args.monitors == 2:
        desktop_pid = test_renderer_crash(args.wallpaper, renderer_for, usage)
    lock_pid = test_renderer_crash(args.lock_wallpaper, renderer_for, usage)
    first = screenshot('recovered-lock-0')
    time.sleep(3)
    second = screenshot('recovered-lock-1')
    for index in range(args.monitors):
        difference = ImageStat.Stat(ImageChops.difference(
            crop_monitor(first, index), crop_monitor(second, index))).mean
        assert max(difference) > 0.3, difference
    print('PASS: lock animation recovers after shared and lock-only renderer crashes', flush=True)
    probe('ShowPrompt')
    wait_for(lambda: json.loads(probe('GetState')[0])['promptVisible'])
    screenshot('lock-prompt')
    setting('paused', 'true')
    wait_for(lambda: usage(lock_pid)[0] == 'T')
    setting('paused', 'false')
    wait_for(lambda: usage(lock_pid)[0] != 'T')
    screen_saver('SetActive', GLib.Variant('(b)', (False,)))
    wait_for(lambda: not screen_saver('GetActive')[0])
    wait_for(lambda: not Path('/proc/%d' % lock_pid).exists())
    print('PASS: independent animated lock backgrounds, pause and unlock cleanup', flush=True)
    (args.output / 'lock-result.json').write_text(json.dumps({
        'passed': True, 'monitor_count': args.monitors,
        'background_difference_rgb': differences,
        'environment': 'isolated GNOME screen shield; no authentication bypass tested',
    }, indent=2) + '\n')
    # Repeated activation catches hooks or clones left over after dialog destruction.
    screen_saver('SetActive', GLib.Variant('(b)', (True,)))
    lock_pid = wait_for(lambda: renderer_for(args.lock_wallpaper))
    wait_for(lambda: screen_saver('GetActive')[0])
    assert 'true' in extension('DisableExtension')
    wait_for(lambda: not Path('/proc/%d' % lock_pid).exists())
    assert 'true' in extension('EnableExtension')
    wait_for(lambda: renderer_for(args.lock_wallpaper))
    screen_saver('SetActive', GLib.Variant('(b)', (False,)))
    wait_for(lambda: not screen_saver('GetActive')[0])
    setting('lock-wallpapers', '[]')
    setting('lock-mirrors', '[]')
    print('PASS: lock background restore on disable and re-enable while locked', flush=True)


def main():
    """Prepare fresh result artifacts and run within a disposable GNOME session."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--extension-source', type=Path, default=SOURCE)
    parser.add_argument('--engine', type=Path, required=True)
    parser.add_argument('--wallpaper', type=Path, required=True)
    parser.add_argument('--assets-dir', type=Path)
    parser.add_argument('--lock-wallpaper', type=Path)
    parser.add_argument('--lock-lifecycle', action='store_true')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--monitors', type=int, choices=(1, 2), default=1)
    parser.add_argument('--isolated', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    for name in ('result.json', 'lock-result.json', 'lifecycle-result.json'):
        (args.output / name).unlink(missing_ok=True)
    args.extension_source = args.extension_source.resolve(strict=True)
    metadata = json.loads((args.extension_source / 'metadata.json').read_text())
    args.uuid = metadata['uuid']
    args.schema = metadata['settings-schema']
    args.engine = args.engine.resolve(strict=True)
    args.wallpaper = args.wallpaper.resolve(strict=True)
    if args.assets_dir:
        args.assets_dir = args.assets_dir.resolve(strict=True)
    if args.lock_wallpaper:
        args.lock_wallpaper = args.lock_wallpaper.resolve(strict=True)
    if args.isolated:
        run_test(args)
        return

    with tempfile.TemporaryDirectory(prefix='lwe-gnome-test-') as directory:
        root = Path(directory)
        env = os.environ.copy()
        for key, name in [('XDG_CONFIG_HOME', 'config'), ('XDG_CACHE_HOME', 'cache'),
                          ('XDG_DATA_HOME', 'data'), ('XDG_RUNTIME_DIR', 'runtime')]:
            path = root / name
            path.mkdir(mode=0o700)
            env[key] = str(path)
        probe_dir = root / 'data/gnome-shell/extensions' / PROBE_UUID
        probe_dir.mkdir(parents=True)
        (probe_dir / 'extension.js').write_text(TEST_PROBE)
        (probe_dir / 'metadata.json').write_text(json.dumps({
            'uuid': PROBE_UUID, 'name': 'Wallpaper test probe',
            'description': 'Isolated integration test only', 'shell-version': ['42'],
            'session-modes': ['user', 'unlock-dialog'],
        }))
        extension = root / 'data/gnome-shell/extensions' / args.uuid
        (extension / 'schemas').mkdir(parents=True)
        for name in ('extension.js', 'metadata.json'):
            (extension / name).symlink_to(args.extension_source / name)
        schema = args.schema + '.gschema.xml'
        (extension / 'schemas' / schema).symlink_to(args.extension_source / 'schemas' / schema)
        subprocess.run(['glib-compile-schemas', '--strict', str(extension / 'schemas')], check=True)
        env.update(GSETTINGS_BACKEND='keyfile', GNOME_SHELL_SESSION_MODE='gnome',
                   WAYLAND_DISPLAY='lwe-smoke', GDK_BACKEND='wayland',
                   SDL_AUDIODRIVER='dummy')
        env.pop('DISPLAY', None)
        command = ['dbus-run-session', '--', sys.executable, str(Path(__file__).resolve()),
                   '--isolated', '--extension-source', str(args.extension_source), '--engine', str(args.engine), '--wallpaper', str(args.wallpaper),
                   '--output', str(args.output), '--monitors', str(args.monitors)]
        if args.lock_lifecycle:
            command.append('--lock-lifecycle')
        if args.lock_wallpaper:
            command.extend(['--lock-wallpaper', str(args.lock_wallpaper)])
        if args.assets_dir:
            command.extend(['--assets-dir', str(args.assets_dir)])
        try:
            subprocess.run(command, env=env, check=True)
        finally:
            # Portal/GVfs mounts can outlive the private session bus. Detach only
            # mounts inside this test's runtime directory before removing it.
            prefix = str(root / 'runtime') + '/'
            mounts = [line.split()[4] for line in Path('/proc/self/mountinfo').read_text().splitlines()
                      if line.split()[4].startswith(prefix)]
            for mount in sorted(mounts, key=len, reverse=True):
                unmount = shutil.which('fusermount3') or shutil.which('fusermount')
                result = subprocess.run([unmount, '-uz', mount], capture_output=True, text=True)
                # The service can unmount itself between our scan and fusermount.
                if result.returncode and any(line.split()[4] == mount for line in
                                             Path('/proc/self/mountinfo').read_text().splitlines()):
                    result.check_returncode()


if __name__ == '__main__':
    main()
