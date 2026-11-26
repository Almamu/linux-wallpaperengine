#!/usr/bin/env python3
"""Capture native/fork temporal video on a private headless Gamescope display.

The native reference adapter supplies private preparation and X11
ownership enumeration. Its path/hash are recorded; there is no display fallback.
"""
import argparse
import ctypes
import fcntl
import importlib.util
import json
import math
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time

from parity import asset_digest, compare, probe, sha256, write_json
from determinism import apply_substitutions, preflight, template

DEFAULT_ADAPTER = Path(__file__).resolve().with_name('native_adapter.py')
LOCK = '/tmp/lwe-render-fix-build.lock'
NATIVE_VOLUMETRICS = 'ultra'  # Fork light volumes use QUALITY 4.


def private_root(path):
    path = path.resolve()
    if not path.is_relative_to('/tmp') or path == Path('/tmp'):
        raise ValueError('Use a private directory below /tmp')
    return path


def adapter_at(path):
    spec = importlib.util.spec_from_file_location('reference_adapter', path)
    adapter = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(adapter)
    return adapter


def private_environment(environ, root):
    runtime = root / 'runtime'
    if runtime.is_symlink():
        raise ValueError('Private runtime directory must not be a symlink')
    runtime.mkdir(mode=0o700, exist_ok=True)
    runtime.chmod(0o700)
    env = dict(environ, LWE_HOST_DISPLAY=environ.get('DISPLAY', ''))
    for key in list(env):
        if key.startswith(('WPE_', 'DBUS_SESSION_BUS_', 'DBUS_STARTER_')) or key in ('DISPLAY', 'WAYLAND_DISPLAY', 'LD_PRELOAD',
                                          'WINEPREFIX', 'LWE_SOCKET'):
            env.pop(key)
    env.update(WPE_CONTROL_SOCKET=str(root / 'control.sock'), WPE_LOG_FILE='off',
               XDG_RUNTIME_DIR=str(runtime),
               PULSE_SERVER='unix:' + str(root / 'no-pulse.sock'),
               PIPEWIRE_REMOTE=str(root / 'no-pipewire.sock'))
    return env


def set_properties(project, overrides):
    metadata = json.loads(project.read_text(encoding='utf-8-sig'))
    definitions = metadata.get('general', {}).get('properties', {})
    if not isinstance(overrides, dict):
        raise ValueError('Property overrides must be a JSON object')
    for key, value in overrides.items():
        if key not in definitions or not isinstance(definitions[key], dict):
            raise ValueError(f'Unknown authored property: {key}')
        if not isinstance(value, (str, int, float, bool)) or isinstance(value, float) and not math.isfinite(value):
            raise ValueError(f'Property {key} needs a finite scalar value')
    for key, value in overrides.items():
        definitions[key]['value'] = value
    if overrides:
        write_json(project, metadata)


def source_manifest(item):
    records = []
    for path in sorted(item.rglob('*')):
        if path.is_symlink():
            raise ValueError(f'Item contains symlink; resolve its asset scope before capture: {path}')
        if path.is_file():
            records.append(dict(path=str(path.relative_to(item)), bytes=path.stat().st_size, sha256=sha256(path)))
    return records


def check_input_boundary(item, expected):
    original = {entry['path']: entry for entry in expected}
    current = {entry['path']: entry for entry in source_manifest(item)}
    if any(current.get(name) != entry for name, entry in original.items()):
        raise ValueError('Private authored wallpaper inputs changed or disappeared')
    generated = []
    for name in sorted(current.keys() - original.keys()):
        if not re.fullmatch(r'shaders/blobsSM40/[0-9a-f]+\.dxs', name):
            raise ValueError(f'Unexpected new private wallpaper input: {name}')
        generated.append(current[name])
    return generated


def prepare(args):
    root = private_root(args.root)
    renderer = getattr(args, 'native_renderer', 'dxvk')
    if renderer not in ('dxvk', 'wined3d'):
        raise ValueError('Select native renderer dxvk or wined3d')
    item = args.item.resolve()
    metadata = json.loads((item / 'project.json').read_text(encoding='utf-8-sig'))
    if metadata.get('type', '').lower() not in ('scene', 'video', 'web'):
        raise ValueError('This item is not a standalone scene, video, or web wallpaper')
    source = source_manifest(item)
    overrides = json.loads(args.properties.read_text()) if args.properties else {}
    adapter = adapter_at(args.adapter)
    adapter.prepare(root, args.width, args.height)
    subprocess.run(['cp', '-a', '--reflink=auto', str(item), str(root / 'item')], check=True)
    set_properties(root / 'item/project.json', overrides)
    config_path = root / 'app/config.json'
    config = json.loads(config_path.read_text())
    # Match the fork's fixed volume sampling and shadow filter quality.
    # Remaining copied controls stay visible and need independent parity evidence.
    config['steamuser']['general']['user'].update(
        fps=args.fps, msaa='none', postprocessing=args.post_processing,
        volumetrics=NATIVE_VOLUMETRICS, shadows='medium',
        autostart=False, autostartscheduler=False, audioinputvolume=0,
        playbackfocus='run', playbackfullscreen='run', playbackmaximized='run')
    config['steamuser']['wproperties'] = {}
    write_json(config_path, config)
    write_json(root / 'capture-config.json', config)
    scenario = dict(schema=1, root=str(root), item=dict(id=item.name,
        title=metadata.get('title', item.name), type=metadata.get('type'),
        project_sha256=sha256(item / 'project.json'), source_path=str(item), asset_digest=asset_digest(source)),
        source_files=source, captured_files=source_manifest(root / 'item'),
        property_overrides=overrides, width=args.width, height=args.height, fps=args.fps,
        config_sha256=sha256(config_path),
        post_processing=args.post_processing, msaa='off', native_renderer=renderer,
        native_volumetrics=NATIVE_VOLUMETRICS,
        adapter=dict(path=str(args.adapter.resolve()), sha256=sha256(args.adapter)),
        caveats=['Private property overrides modify both copies identically; source assets remain unchanged.',
                 'Web network/storage/browser clocks and media integration are uncontrolled.',
                 'Audio servers are disconnected; equivalent audio behavior has not been established.',
                 'No shared simulation clock or random seed. Matching FPS/warmup is not synchronization.',
                 'Native popout clipping and color transfer require independent calibration.'])
    write_json(root / 'scenario.json', scenario)
    write_json(root / 'controls.json', template({key: scenario['item'][key] for key in
                                               ('id', 'project_sha256', 'asset_digest')}))
    return scenario


def validate_native_quality(root, scenario):
    user = json.loads((root / 'app/config.json').read_text())['steamuser']['general']['user']
    if (scenario.get('native_volumetrics') != NATIVE_VOLUMETRICS
            or user.get('volumetrics') != NATIVE_VOLUMETRICS or user.get('shadows') != 'medium'):
        raise ValueError('Prepared native quality must be volumetrics=ultra, shadows=medium; use a new private root')


def video_command(display, xid, width, height, fps, frames, output):
    return ['ffmpeg', '-nostdin', '-hide_banner', '-loglevel', 'verbose', '-debug_ts',
            '-f', 'x11grab', '-framerate', str(fps), '-draw_mouse', '0',
            '-window_id', str(int(xid, 16)), '-video_size', f'{width}x{height}',
            '-i', display, '-map', '0:v:0', '-an', '-frames:v', str(frames),
            '-fps_mode', 'passthrough', '-c:v', 'ffv1', '-level', '3',
            '-pix_fmt', 'bgr0', str(output)]


def stop_group(proc):
    if proc is None:
        return
    # The leader may already have exited with descendants still in its group.
    try:
        os.killpg(proc.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    try:
        proc.wait(timeout=3)
    except subprocess.TimeoutExpired:
        os.killpg(proc.pid, signal.SIGKILL)
        proc.wait(timeout=3)
    try:
        os.killpg(proc.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass


def native_environment(env, root, adapter, renderer='dxvk'):
    if renderer not in ('dxvk', 'wined3d'):
        raise ValueError('Scenario must select native renderer dxvk or wined3d; prepare again')
    return dict(env, STEAM_COMPAT_DATA_PATH=str(root / 'compat'),
        STEAM_COMPAT_CLIENT_INSTALL_PATH=str(adapter.STEAM), SteamAppId='431960',
        SteamGameId='431960', PROTON_LOG='1', PROTON_LOG_DIR=str(root),
        PROTON_ENABLE_WAYLAND='0', DXVK_LOG_PATH=str(root),
        PROTON_USE_WINED3D='1' if renderer == 'wined3d' else '0', PROTON_USE_WINED3D11='0',
        WINEDLLOVERRIDES='d3d11,dxgi=n,b' if renderer == 'dxvk' else 'd3d11,dxgi=b')


def stop_native(root, adapter, env):
    return subprocess.run([str(adapter.PROTON / 'files/bin/wineserver'), '-k'],
        env=dict(env, WINEPREFIX=str(root / 'compat/pfx')), capture_output=True, timeout=5).returncode


def owns_prefix(variables, root):
    return any(value.startswith(b'WINEPREFIX=') and
               value.split(b'=', 1)[1].rstrip(b'/') == str(root / 'compat/pfx').encode()
               for value in variables)


def mapped_identity(pid, role):
    paths = set()
    for line in (Path('/proc') / str(pid) / 'maps').read_text().splitlines():
        fields = line.split(maxsplit=5)
        if len(fields) == 6 and fields[5].startswith('/'):
            paths.add(fields[5])
    selected = sorted(path for path in paths if re.search(
        r'linux-wallpaperengine|wallpaper64\.exe|libnvidia|libGLX|libEGL|libvulkan|wined3d|d3d11|dxgi', path))
    if role == 'candidate' and not any('liblinux-wallpaperengine-lib.so' in path for path in selected):
        raise RuntimeError('Loaded fork renderer library identity is unavailable')
    if any(path.endswith(' (deleted)') for path in selected):
        raise RuntimeError('Capture process maps a deleted renderer/driver artifact: ' + ', '.join(selected))
    return [dict(path=path, sha256=sha256(path)) for path in selected]


def native_renderer_sources(proton, renderer):
    wine = proton / 'files/lib/wine'
    if renderer == 'dxvk':
        return [wine / 'dxvk/x86_64-windows' / name for name in ('d3d11.dll', 'dxgi.dll')]
    if renderer == 'wined3d':
        return [wine / 'x86_64-windows' / name for name in ('d3d11.dll', 'dxgi.dll', 'wined3d.dll')]
    raise ValueError('Native renderer selection is missing or invalid')


def verify_native_renderer(artifacts, renderer, root, adapter):
    names = ('d3d11.dll', 'dxgi.dll', 'wined3d.dll')
    sources = native_renderer_sources(adapter.PROTON, renderer)
    system32 = root / 'compat/pfx/drive_c/windows/system32'
    actual = sorted((entry for entry in artifacts if Path(entry['path']).name.lower() in names),
                    key=lambda entry: Path(entry['path']).name.lower())
    if len(actual) != len(sources):
        raise RuntimeError(f'Native renderer {renderer} did not load the expected DLL paths and hashes; refusing fallback')
    for entry, source in zip(actual, sources):
        allowed = {str(system32 / source.name)}
        if renderer == 'wined3d':
            allowed.add(str(source.resolve(strict=True)))
        if entry['path'] not in allowed or entry['sha256'] != sha256(source):
            raise RuntimeError(f'Native renderer {renderer} loaded an unexpected {source.name}; refusing fallback')
    return dict(renderer=renderer, libraries=actual)


def owned_window(x11, root, role, proc):
    for xid in x11.named():
        props = subprocess.run(['xprop', '-id', xid, '_NET_WM_PID'],
                               capture_output=True, text=True, timeout=3).stdout
        match = re.search(r'= (\d+)', props)
        if not match:
            continue
        pid = int(match[1])
        try:
            if role == 'reference':
                variables = (Path('/proc') / str(pid) / 'environ').read_bytes().split(b'\0')
                owned = owns_prefix(variables, root)
            else:
                owned = pid == proc.pid
        except (FileNotFoundError, ProcessLookupError):
            continue
        if not owned:
            raise RuntimeError(f'Refusing unowned {role} XID {xid}, PID {pid}')
        return xid, pid
    return None


def wait_scene_ready(log_path, marker, proc, role, deadline, start_offset=0, start_inode=None):
    """Observe the fixture's first update in new log bytes, not frame presentation."""
    offset, inode, pending = start_offset, start_inode, b''
    pattern = re.compile(rb'(?:^|[ \t])' + re.escape(marker.encode()) + rb' (.+)$')
    while time.monotonic() < deadline:
        code = proc.poll()
        if code not in (None, 0) or role == 'candidate' and code is not None:
            raise RuntimeError(f'{role} exited before its scene update marker: {code}')
        try:
            with log_path.open('rb') as stream:
                info = os.fstat(stream.fileno())
                if (inode is not None and inode != info.st_ino) or info.st_size < offset:
                    offset, pending = 0, b''
                inode = info.st_ino
                stream.seek(offset)
                chunk = stream.read(65536)
                offset += len(chunk)
        except FileNotFoundError:
            chunk = b''
        lines = (pending + chunk).split(b'\n')
        pending = lines.pop()
        for line in lines:
            match = pattern.search(line.rstrip(b'\r'))
            if not match:
                continue
            data = json.loads(match[1])
            if not isinstance(data, dict) or any(
                    isinstance(data.get(key), bool) or not isinstance(data.get(key), (int, float))
                    or not math.isfinite(data[key]) or data[key] < 0 for key in ('runtime', 'at')):
                raise ValueError(f'Invalid {role} scene update marker payload')
            return data
        time.sleep(min(.02, max(0, deadline - time.monotonic())))
    raise TimeoutError(f'{role} readiness timeout; no new scene update marker {marker}')


def worker(args, scenario):
    root, out, role = args.root, args.out, args.worker
    validate_native_quality(root, scenario)
    adapter = adapter_at(Path(scenario['adapter']['path']))
    adapter.validate_display(os.environ.get('DISPLAY', ''), os.environ.get('LWE_HOST_DISPLAY', ''))
    if (root / 'app/bin/wallpaperui.exe').exists():
        raise ValueError('Private selector must be omitted')
    env = dict(os.environ)
    env.pop('WAYLAND_DISPLAY', None)
    env.update(XDG_SESSION_TYPE='x11', SDL_VIDEODRIVER='x11')
    width, height, fps = (scenario[key] for key in ('width', 'height', 'fps'))
    adapter.TITLE = 'LWE Private Native Probe' if role == 'reference' else 'wallpaperengine'
    if role == 'reference':
        env = native_environment(env, root, adapter, scenario.get('native_renderer'))
        command = [str(adapter.PROTON / 'proton'), 'runinprefix', adapter.win(root / 'app/wallpaper64.exe'),
                   '-silent', '-nowallpapers', '-control', 'openWallpaper', '-file',
                   adapter.win(root / 'item/project.json'), '-playInWindow', adapter.TITLE,
                   '-width', str(width), '-height', str(height), '-borderless']
    else:
        env['SDL_AUDIODRIVER'] = 'dummy'
        command = [str(args.binary), '--window', f'0x0x{width}x{height}', '--fps', str(fps),
                   '--assets-dir', str(root / 'app/assets'), '--msaa', 'off',
                   '--post-processing', scenario['post_processing'], '--render-scale', '1',
                   '--scaling', 'fill', '--silent', '--no-audio-processing', '--noautomute',
                   '--no-fullscreen-pause', str(root / 'item')]
    result = dict(schema=1, role=role, command=command, display=env['DISPLAY'],
                  host_display=env.get('LWE_HOST_DISPLAY'), status='capture_failed')
    config_before = (root / 'app/config.json').read_bytes()
    native_log = root / 'app/log.txt'
    (root / 'control.sock').unlink(missing_ok=True)
    proc = None
    x11 = adapter.X11()
    try:
        expected_assets = asset_digest(scenario['captured_files'])
        previous_cache = check_input_boundary(root / 'item', scenario['captured_files'])
        result['removed_previous_generated_shader_cache'] = previous_cache
        for entry in previous_cache:
            (root / 'item' / entry['path']).unlink()
        if scenario.get('config_sha256') and sha256(root / 'app/config.json') != scenario['config_sha256']:
            raise RuntimeError('Private native configuration changed after preparation')
        result.update(input_asset_digest=expected_assets, config_sha256=sha256(root / 'app/config.json'),
                      scenario_sha256=sha256(out / 'scenario.json'))
        # Set a fixed pointer location on this private display only; this does
        # not prove identical application pointer/control-point handling.
        x11.lib.XWarpPointer.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_ulong,
                                         ctypes.c_int, ctypes.c_int, ctypes.c_uint, ctypes.c_uint,
                                         ctypes.c_int, ctypes.c_int]
        x11.lib.XFlush.argtypes = [ctypes.c_void_p]
        x11.lib.XWarpPointer(x11.display, 0, x11.root, 0, 0, 0, 0, width // 2, height // 2)
        x11.lib.XFlush(x11.display)
        if role == 'reference':
            # The app can truncate and refill its log before we get an XID.
            # Begin with a fresh private log regardless of its open mode.
            native_log.unlink(missing_ok=True)
        with (out / f'{role}-engine.log').open('w') as log:
            result['launch_monotonic_ns'] = time.monotonic_ns()
            proc = subprocess.Popen(command, cwd=root / 'app' if role == 'reference' else args.binary.parent,
                                    env=env, stdout=log, stderr=subprocess.STDOUT,
                                    start_new_session=True, close_fds=True)
            deadline = time.monotonic() + args.startup_timeout
            found = None
            while time.monotonic() < deadline:
                found = owned_window(x11, root, role, proc)
                if found and x11.size(found[0]) == [width, height]:
                    if role == 'reference' or (root / 'control.sock').exists():
                        break
                if proc.poll() not in (None, 0) or role == 'candidate' and proc.poll() is not None:
                    raise RuntimeError(f'{role} exited before readiness: {proc.returncode}')
                time.sleep(.1)
            else:
                raise TimeoutError(f'{role} readiness timeout; no owned full-size drawable')
            result.update(xid=found[0], pid=found[1], window_ready_monotonic_ns=time.monotonic_ns())
            marker = scenario.get('fixture', {}).get('ready_marker')
            if marker:
                ready_log = native_log if role == 'reference' else out / f'{role}-engine.log'
                payload = wait_scene_ready(ready_log, marker, proc, role, deadline,
                    start_inode=None if role == 'reference' else os.fstat(log.fileno()).st_ino)
                result.update(scene_ready_monotonic_ns=time.monotonic_ns(),
                    scene_ready=dict(marker=marker, log_path=str(ready_log), payload=payload,
                        scope='First fixture SceneScript update; not a presentation fence or visual parity proof.'))
            result['warmup_after'] = 'scene_first_update' if marker else 'owned_window'
            time.sleep(args.warmup)
            if owned_window(x11, root, role, proc) != found or x11.size(found[0]) != [width, height]:
                raise RuntimeError('Owned window changed before capture')
            # Native creates its window before loading the graphics driver.
            # Bind the recording after warmup, then verify its closing boundary.
            result['mapped_artifacts'] = mapped_identity(found[1], role)
            if role == 'reference':
                result['native_renderer_verified'] = verify_native_renderer(
                    result['mapped_artifacts'], scenario['native_renderer'], root, adapter)
            output = out / f'{role}.mkv'
            frames = round(args.duration * fps)
            recording = video_command(env['DISPLAY'], found[0], width, height, fps, frames, output)
            result.update(recording_command=recording,
                          acquisition_launch_realtime_ns=time.time_ns(),
                          acquisition_launch_monotonic_ns=time.monotonic_ns())
            with (out / f'{role}-ffmpeg.log').open('w') as video_log:
                subprocess.run(recording, env=env, stdout=video_log, stderr=subprocess.STDOUT,
                               timeout=args.duration * 3 + 15, check=True)
            result['acquisition_complete_monotonic_ns'] = time.monotonic_ns()
            info = probe(output)
            if (info['width'], info['height']) != (width, height) or len(info['timestamps']) != frames:
                raise RuntimeError('Encoded video geometry/frame count differs from requested capture')
            if owned_window(x11, root, role, proc) != found or x11.size(found[0]) != [width, height]:
                raise RuntimeError('Owned window changed during capture')
            result['mapped_artifacts_after_capture'] = mapped_identity(found[1], role)
            if result['mapped_artifacts_after_capture'] != result['mapped_artifacts']:
                raise RuntimeError('Loaded renderer/driver artifacts changed during capture')
            result['generated_shader_cache'] = check_input_boundary(root / 'item', scenario['captured_files'])
            result['inputs_unchanged_after_capture'] = True
            if role == 'reference' and native_log.exists():
                new_log = native_log.read_bytes()
                (out / 'reference-application.log').write_bytes(new_log)
                if re.search(rb'shader[^\n]*error:|error X\d{4}', new_log, re.I):
                    raise RuntimeError('Native shader compilation failed; inspect reference-application.log')
            result.update(status='captured', video=dict(path=str(output), sha256=sha256(output), **info),
                timing_scope='PTS are X11 capture samples. FFmpeg debug_ts retains input acquisition timestamps; launch clocks bound process startup, not simulation origin. No presentation fence/exposure synchronization.')
    except Exception as error:
        result['error'] = str(error)
    finally:
        try:
            if role == 'reference':
                result['native_cleanup_returncode'] = stop_native(root, adapter, env)
                if result['native_cleanup_returncode']:
                    raise RuntimeError('Private native prefix cleanup failed')
        except Exception as error:
            result.update(status='capture_failed', cleanup_error=str(error))
        stop_group(proc)
        x11.close()
        if role == 'reference':
            (out / 'native-config-after-run.json').write_bytes((root / 'app/config.json').read_bytes())
            (root / 'app/config.json').write_bytes(config_before)
        (root / 'control.sock').unlink(missing_ok=True)
        write_json(out / f'{role}.json.tmp', result)
        (out / f'{role}.json.tmp').replace(out / f'{role}.json')
    return result


def run_pair(args, scenario):
    if scenario.get('native_renderer') not in ('dxvk', 'wined3d'):
        raise ValueError('Scenario must select native renderer dxvk or wined3d; prepare again')
    validate_native_quality(args.root, scenario)
    out = private_root(args.out)
    out.mkdir(parents=True, exist_ok=False)
    root = args.root
    adapter = adapter_at(Path(scenario['adapter']['path']))
    env = private_environment(os.environ, root)
    report = dict(schema=1, item=scenario['item'], status='capture_failed', blockers=[])
    write_json(out / 'scenario.json', scenario)
    controls = json.loads((root / 'controls.json').read_text()) if (root / 'controls.json').exists() else template(scenario['item'])
    control_report = preflight(controls)
    write_json(out / 'preflight.json', control_report)
    write_json(out / 'controls.json', controls)
    if control_report['status'] == 'blocked' and not args.diagnostic:
        report.update(status='determinism_blocked', blockers=control_report['blockers'])
        write_json(out / 'comparison.json', report)
        return report
    # Lock is private to this supervisor and never inherited by descendants.
    with open(LOCK, 'w') as lock:
        os.set_inheritable(lock.fileno(), False)
        fcntl.flock(lock, fcntl.LOCK_EX)
        if sha256(Path(scenario['adapter']['path'])) != scenario['adapter']['sha256']:
            raise ValueError('Native adapter changed since preparation; prepare again')
        write_json(out / 'binaries.json', dict(native=sha256(root / 'app/wallpaper64.exe'),
                   native_renderer=scenario['native_renderer'],
                   candidate=sha256(args.binary), candidate_path=str(args.binary),
                   native_base_assets=asset_digest(source_manifest(root / 'app/assets')),
                   proton=sha256(adapter.PROTON / 'proton'),
                   proton_version=(adapter.PROTON / 'version').read_text() if (adapter.PROTON / 'version').exists() else None))
        try:
            for role in ('reference', 'candidate'):
                command = ['dbus-run-session', '--', 'gamescope', '--backend', 'headless', '--disable-color-management',
                    '-W', str(scenario['width']),
                    '-H', str(scenario['height']), '-w', str(scenario['width']), '-h', str(scenario['height']),
                    '--', sys.executable, str(Path(__file__).resolve()), 'pair', '--root', str(root),
                    '--out', str(out), '--binary', str(args.binary), '--duration', str(args.duration),
                    '--warmup', str(args.warmup), '--startup-timeout', str(args.startup_timeout), '--worker', role]
                write_json(out / f'{role}-headless-command.json', command)
                with (out / f'{role}-gamescope.log').open('w') as log:
                    game = subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT,
                                            start_new_session=True, close_fds=True)
                    try:
                        deadline = time.monotonic() + args.startup_timeout + args.warmup + args.duration * 3 + 30
                        result_path = out / f'{role}.json'
                        while not result_path.exists() and time.monotonic() < deadline:
                            if game.poll() is not None:
                                raise RuntimeError(f'{role} headless display exited: {game.returncode}')
                            time.sleep(.2)
                        if not result_path.exists():
                            raise TimeoutError(f'{role} private capture timed out')
                        result = json.loads(result_path.read_text())
                        if result['status'] != 'captured':
                            raise RuntimeError(result.get('error', result.get('cleanup_error', 'Capture failed')))
                    finally:
                        try:
                            if role == 'reference':
                                stop_native(root, adapter, env)
                        finally:
                            stop_group(game)
            report = compare(out / 'reference.mkv', out / 'candidate.mkv', out=out, controls=controls)
            report['item'] = scenario['item']
        except Exception as error:
            report['blockers'].append(str(error))
        finally:
            write_json(out / 'comparison.json', report)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    prep = commands.add_parser('prepare', help='Create private application, prefix and wallpaper copies')
    prep.add_argument('--root', type=Path, required=True)
    prep.add_argument('--item', type=Path, required=True)
    prep.add_argument('--adapter', type=Path, default=DEFAULT_ADAPTER)
    prep.add_argument('--width', type=int, default=960)
    prep.add_argument('--height', type=int, default=540)
    prep.add_argument('--fps', type=int, default=30)
    prep.add_argument('--post-processing', choices=['disabled', 'enabled', 'ultra'], default='enabled')
    prep.add_argument('--native-renderer', choices=['dxvk', 'wined3d'], default='dxvk')
    prep.add_argument('--properties', type=Path)
    apply = commands.add_parser('apply-controls', help='Apply reviewed exact substitutions to private loose inputs')
    apply.add_argument('--root', type=Path, required=True)
    apply.add_argument('--controls', type=Path, required=True)
    pair = commands.add_parser('pair', help='Record both renderers sequentially under the shared GPU lock')
    pair.add_argument('--root', type=Path, required=True)
    pair.add_argument('--out', type=Path, required=True)
    pair.add_argument('--binary', type=Path, required=True)
    pair.add_argument('--duration', type=float, default=8)
    pair.add_argument('--warmup', type=float, default=5)
    pair.add_argument('--startup-timeout', type=float, default=60)
    pair.add_argument('--worker', choices=['reference', 'candidate'], help=argparse.SUPPRESS)
    pair.add_argument('--diagnostic', action='store_true', help='Allow exploratory capture while input controls remain unresolved')
    args = parser.parse_args()
    args.root = private_root(args.root)
    if args.command == 'prepare':
        if not (64 <= args.width <= 7680 and 64 <= args.height <= 4320 and 1 <= args.fps <= 240):
            parser.error('Invalid dimensions or FPS')
        result = prepare(args)
    elif args.command == 'apply-controls':
        controls = json.loads(args.controls.read_text())
        scenario = json.loads((args.root / 'scenario.json').read_text())
        identity = {key: scenario['item'][key] for key in ('id', 'project_sha256', 'asset_digest')}
        if controls.get('item') != identity:
            parser.error('Control manifest does not match the prepared source item')
        generated_cache = check_input_boundary(args.root / 'item', scenario['captured_files'])
        for entry in generated_cache:
            (args.root / 'item' / entry['path']).unlink()
        scenario['removed_before_controls_cache'] = generated_cache
        scenario['input_substitutions'] = apply_substitutions(args.root, controls)
        scenario['captured_files'] = source_manifest(args.root / 'item')
        scenario['config_sha256'] = sha256(args.root / 'app/config.json')
        write_json(args.root / 'scenario.json', scenario)
        controls['scenario_sha256'] = sha256(args.root / 'scenario.json')
        write_json(args.root / 'controls.json', controls)
        result = preflight(controls)
        write_json(args.root / 'preflight.json', result)
    else:
        if not (0 < args.duration <= 300 and 0 <= args.warmup <= 60 and 1 <= args.startup_timeout <= 300):
            parser.error('Use duration (0,300], warmup [0,60], startup timeout [1,300] seconds')
        args.binary = args.binary.resolve(strict=True)
        args.out = private_root(args.out)
        scenario = json.loads((args.root / 'scenario.json').read_text())
        if scenario['root'] != str(args.root):
            parser.error('Scenario does not belong to this private root')
        if round(args.duration * scenario['fps']) < 2:
            parser.error('Capture at least two frames')
        result = worker(args, scenario) if args.worker else run_pair(args, scenario)
    print(json.dumps({key: result[key] for key in ('status', 'item', 'root', 'blockers') if key in result}, indent=2))
    return 1 if result.get('status') in ('capture_failed', 'determinism_blocked', 'blocked') else 0


if __name__ == '__main__':
    sys.exit(main())
