#!/usr/bin/env python3
"""Capture a generated Deep Sea fixture manifest with one private app/prefix.

All captures are diagnostic. Neither frozen JSON nor a low image error proves
native control application, temporal stability, framing, or rendering parity.
"""
import argparse
from collections import Counter
import fcntl
import hashlib
import html
import json
import math
from pathlib import Path
import re
import shutil
import subprocess
import sys

import numpy as np
from PIL import Image, ImageDraw

import capture
import determinism
import parity
from determinism import template
from parity import asset_digest, sha256, write_json

SCOPE = ('Diagnostic frozen-fixture captures only. Native control application, same-engine '
         'repeatability, quality, framing and color gates remain unverified. No parity verdict.')


def measure_still(reference, candidate, difference, tolerance=2):
    """Measure the selected matched frame without resizing, cropping or aligning."""
    with Image.open(reference) as image:
        a = np.asarray(image.convert('RGB'), dtype=np.float64)
    with Image.open(candidate) as image:
        b = np.asarray(image.convert('RGB'), dtype=np.float64)
    if a.shape != b.shape:
        raise ValueError('Still image geometry differs; resizing is forbidden')
    delta = np.abs(a - b)
    mse = float(np.square(delta).mean())
    Image.fromarray(delta.astype(np.uint8)).save(difference)
    return dict(mae_rgb8=float(delta.mean()), rmse_rgb8=math.sqrt(mse),
                psnr_db=10 * math.log10(255 ** 2 / mse) if mse else None,
                psnr_infinite=mse == 0,
                changed_pixel_fraction=float(np.any(delta > tolerance, axis=2).mean()),
                width=a.shape[1], height=a.shape[0], pixel_tolerance_rgb8=tolerance)


def fixture_files(entry, root, out):
    if Path(entry['path']).is_symlink():
        raise ValueError('Fixture source directory must not be a symlink')
    item = Path(entry['path']).resolve(strict=True)
    if any(item.is_relative_to(path) or path.is_relative_to(item) for path in (root, out)):
        raise ValueError('Fixture source must be separate from private root and results')
    records = capture.source_manifest(item)
    if asset_digest(records) != entry['asset_digest']:
        raise ValueError('Fixture asset digest differs from generated manifest')
    return item, records


def private_processes(root):
    """Find surviving workers before changing their item files; do not kill them."""
    owned = []
    markers = {f'WPE_CONTROL_SOCKET={root}/control.sock'.encode(),
               f'STEAM_COMPAT_DATA_PATH={root}/compat'.encode()}
    for process in Path('/proc').glob('[0-9]*'):
        try:
            variables = (process / 'environ').read_bytes().split(b'\0')
        except (FileNotFoundError, ProcessLookupError, PermissionError):
            continue
        if capture.owns_prefix(variables, root) or markers.intersection(variables):
            owned.append(int(process.name))
    return owned


def finish_fixture_swap(root):
    swap = root / '.fixture-swap'
    if swap.is_symlink():
        raise ValueError('Private fixture swap directory must not be a symlink')
    if not swap.exists():
        return
    target = root / 'item'
    previous = swap / 'previous-item'
    # The scenario rename is the commit point. Until it happens, put the old
    # item and controls back so the next row can use the previous scenario.
    if previous.exists() and (swap / 'next-scenario.json').exists():
        if target.exists():
            target.rename(swap / 'next-item')
        backup = swap / 'previous-controls.json'
        if backup.exists():
            restored = swap / 'restore-controls.json'
            shutil.copyfile(backup, restored)
            restored.replace(root / 'controls.json')
        elif not (swap / 'next-controls.json').exists():
            (root / 'controls.json').unlink(missing_ok=True)
        previous.rename(target)
    shutil.rmtree(swap)


def install_fixture(root, scenario, entry, item, records, manifest_sha256):
    """Replace only this sweep's private item; retain the prepared native app/prefix."""
    target = root / 'item'
    if target.is_symlink() or target.resolve().parent != root:
        raise ValueError('Private item boundary escaped the owned sweep root')
    processes = private_processes(root)
    if processes:
        raise RuntimeError(f'Private capture processes still running; item left untouched: {processes}')
    finish_fixture_swap(root)
    capture.check_input_boundary(target, scenario['captured_files'])
    swap = root / '.fixture-swap'
    swap.mkdir(mode=0o700)
    stage = swap / 'next-item'
    controls = root / 'controls.json'
    try:
        subprocess.run(['cp', '-a', '--reflink=auto', str(item), str(stage)], check=True)
        if capture.source_manifest(stage) != records:
            raise ValueError('Fixture changed while being copied')
        project = stage / 'project.json'
        metadata = json.loads(project.read_text(encoding='utf-8-sig'))
        project_hash = sha256(project)
        capture.set_properties(project, entry.get('properties', {}))
        scenario = dict(scenario, item=dict(id=entry['id'], title=metadata.get('title', entry['id']),
            type=metadata.get('type'), source_path=str(item), project_sha256=project_hash,
            asset_digest=entry['asset_digest']), source_files=records, captured_files=capture.source_manifest(stage),
            property_overrides=entry.get('properties', {}), fixture=entry, fixture_manifest_sha256=manifest_sha256)
        identity = {key: scenario['item'][key] for key in ('id', 'project_sha256', 'asset_digest')}
        write_json(swap / 'next-scenario.json', scenario)
        write_json(swap / 'next-controls.json', template(identity))
        if controls.exists():
            shutil.copyfile(controls, swap / 'previous-controls.json')
        target.rename(swap / 'previous-item')
        stage.rename(target)
        (swap / 'next-controls.json').replace(controls)
        (swap / 'next-scenario.json').replace(root / 'scenario.json')
    finally:
        finish_fixture_swap(root)
    return scenario


def shared_identity(root, scenario):
    adapter = capture.adapter_at(Path(scenario['adapter']['path']))
    renderer = scenario['native_renderer']
    dlls = capture.native_renderer_sources(adapter.PROTON, renderer)
    return dict(native=sha256(root / 'app/wallpaper64.exe'),
        base_assets=asset_digest(capture.source_manifest(root / 'app/assets')),
        config=sha256(root / 'app/config.json'), adapter=sha256(Path(scenario['adapter']['path'])),
        proton=sha256(adapter.PROTON / 'proton'), native_renderer=renderer,
        proton_dlls={str(path.resolve()): sha256(path) for path in dlls})


def can_resume(row, key):
    if not isinstance(row, dict) or row.get('status') != 'captured' or row.get('resume_key') != key:
        return False
    # Bind footage, full-size images, metadata and the actually loaded libraries.
    artifacts, runtime = row.get('artifacts'), row.get('runtime_artifacts')
    if not all(isinstance(records, list) and records for records in (artifacts, runtime)):
        return False
    try:
        return all(sha256(Path(record['path'])) == record['sha256'] for record in artifacts + runtime)
    except (OSError, KeyError, TypeError):
        return False


def captured_result(row, run, comparison, key):
    if comparison.get('status') not in ('exploratory', 'comparable_difference', 'no_difference_in_capture'):
        raise RuntimeError('; '.join(comparison.get('blockers', [])) or comparison.get('status', 'Capture failed'))
    images = {name: run / file for name, file in (
        ('native', 'worst-reference.png'), ('fork', 'worst-candidate.png'),
        ('difference', 'worst-absolute-difference.png'))}
    still = measure_still(images['native'], images['fork'], images['difference'],
                          comparison.get('pixel_tolerance_rgb8', 2))
    runtime = []
    for role in ('reference', 'candidate'):
        metadata = json.loads((run / f'{role}.json').read_text())
        before, after = metadata.get('mapped_artifacts'), metadata.get('mapped_artifacts_after_capture')
        if metadata.get('status') != 'captured' or not before or before != after:
            raise ValueError(f'{role}: missing stable loaded binary identities')
        if metadata.get('cleanup_error') or metadata.get('native_cleanup_returncode', 0):
            raise ValueError(f'{role}: private cleanup failed')
        runtime.extend(after)
    artifacts = [dict(path=str(path), sha256=sha256(path)) for path in sorted(run.iterdir()) if path.is_file()]
    return dict(row, status='captured', run=str(run), resumed=False, resume_key=key,
        comparison_status=comparison['status'], blockers=comparison.get('blockers', []),
        metrics=comparison['metrics'], still_metrics=still,
        images={name: str(path) for name, path in images.items()},
        artifacts=artifacts, runtime_artifacts=runtime,
        still_scope='Worst matched frame by full-resolution MAE, as selected by capture.py; difference gain 1.')


def write_report(out, report, sheets=False):
    report['counts'] = dict(Counter(row['status'] for row in report['results']))
    write_json(out / 'report.json', report)
    if not sheets:
        return
    pages = []
    rows = report['results']
    for start in range(0, len(rows), 12):
        batch = rows[start:start + 12]
        sheet = Image.new('RGB', (984, 48 + 214 * len(batch)), '#151b23')
        draw = ImageDraw.Draw(sheet)
        for column, label in enumerate(('Native', 'Fork', 'Absolute difference (1x)')):
            draw.text((12 + column * 324, 14), label, fill='white')
        for index, row in enumerate(batch):
            y = 48 + 214 * index
            label = f"{row['id']} | {row['status']}"
            if row['status'] == 'captured':
                label += f" | frame MAE {row['still_metrics']['mae_rgb8']:.6g} | diagnostic"
                for column, name in enumerate(('native', 'fork', 'difference')):
                    with Image.open(row['images'][name]) as source:
                        thumb = source.convert('RGB')
                    thumb.thumbnail((312, 176), Image.Resampling.LANCZOS)
                    sheet.paste(thumb, (12 + 324 * column, y + 24))
            else:
                draw.text((12, y + 34), row.get('error', 'Capture pending')[:145], fill='#ff9999')
            draw.text((12, y), label[:150], fill='white')
        name = f'contact-sheet-{len(pages) + 1:03d}.png'
        sheet.save(out / name)
        pages.append(name)
    report['contact_sheets'] = pages
    write_json(out / 'report.json', report)
    table = []
    for row in rows:
        score = row.get('metrics', {}).get('mae_rgb8')
        detail = row.get('error', '; '.join(row.get('blockers', [])))
        link = ''
        if row.get('run') and (Path(row['run']) / 'comparison.json').exists():
            relative = str(Path(row['run']).relative_to(out))
            link = f'<a href="{html.escape(relative)}/comparison.json">capture</a>'
        table.append(f'<tr><td>{html.escape(row["id"])}</td><td>{row["status"]}</td>'
                     f'<td>{score if score is not None else "—"}</td><td>{link}</td>'
                     f'<td>{html.escape(detail)}</td></tr>')
    (out / 'report.html').write_text('<!doctype html><meta charset="utf-8"><title>Deep Sea diagnostic sweep</title>'
        '<style>body{font:15px sans-serif;background:#151b23;color:#eee;margin:24px}a{color:#8cf}'
        'td,th{padding:8px;border-bottom:1px solid #456;text-align:left}img{max-width:100%}</style>'
        '<h1>Deep Sea diagnostic sweep</h1><p>' + html.escape(SCOPE) + '</p><p>'
        + html.escape(json.dumps(report['counts'])) + '</p><p>Scores use original RGB8 pixels. '
        'Only contact-sheet thumbnails are resized. Video metrics and all blockers are in '
        '<a href="report.json">report.json</a>.</p><table><tr><th>Fixture</th><th>Capture</th>'
        '<th>Video MAE</th><th>Evidence</th><th>Limits / error</th></tr>' + ''.join(table) + '</table>'
        + ''.join(f'<p><a href="{page}"><img src="{page}" alt="Native, fork and difference contact sheet"></a></p>'
                  for page in pages))


def run_sweep(args):
    args.manifest = args.manifest.resolve(strict=True)
    manifest_bytes = args.manifest.read_bytes()
    manifest = json.loads(manifest_bytes)
    if manifest.get('schema') != 1 or not isinstance(manifest.get('fixtures'), list):
        raise ValueError('Expected a schema 1 generated fixture manifest')
    entries = manifest['fixtures']
    ids = [entry['id'] for entry in entries]
    if len(set(ids)) != len(ids) or any(not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*', name) for name in ids):
        raise ValueError('Fixture IDs must be unique safe directory names')
    unknown = set(args.fixture) - set(ids)
    if unknown:
        raise ValueError('Unknown fixture IDs: ' + ', '.join(sorted(unknown)))
    entries = [entry for entry in entries if not args.fixture or entry['id'] in args.fixture]
    if not entries:
        raise ValueError('No fixtures selected')
    root, out = capture.private_root(args.root), capture.private_root(args.out)
    args.root, args.out = root, out
    args.binary = args.binary.resolve(strict=True)
    args.adapter = args.adapter.resolve(strict=True)
    if root.is_relative_to(out) or out.is_relative_to(root):
        raise ValueError('Private root and results must be separate directories')
    for entry in manifest['fixtures']:
        source = Path(entry['path']).resolve()
        if any(source.is_relative_to(path) or path.is_relative_to(source) for path in (root, out)):
            raise ValueError('Fixture sources must be separate from private root and results')
    settings = {key: getattr(args, key) for key in ('width', 'height', 'fps', 'post_processing', 'native_renderer')}
    settings['native_volumetrics'] = capture.NATIVE_VOLUMETRICS
    owner = dict(manifest=str(args.manifest), settings=settings, adapter=str(args.adapter))
    state_path = root / 'sweep-state.json'
    root.parent.mkdir(parents=True, exist_ok=True)
    out.parent.mkdir(parents=True, exist_ok=True)
    with (root.parent / f'.{root.name}.sweep.lock').open('a') as root_lock, \
         (out.parent / f'.{out.name}.sweep.lock').open('a') as output_lock:
        for lock in (root_lock, output_lock):
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        if root.exists():
            previous_owner = json.loads(state_path.read_text()) if state_path.is_file() else {}
            # Earlier sweeps used WineD3D. Do not relabel their roots or clips.
            previous_owner.setdefault('settings', {}).setdefault('native_renderer', 'wined3d')
            if not args.resume or previous_owner != owner:
                raise ValueError('Refusing an existing private root not owned by this matching resumed sweep')
            if (root / '.fixture-swap').exists() and private_processes(root):
                raise RuntimeError('Private capture processes still running; fixture swap left untouched')
            finish_fixture_swap(root)
        if out.exists() and not args.resume:
            raise ValueError('Results directory exists; use --resume or a new output path')
        out.mkdir(exist_ok=True)
        return _run(args, entries, owner, state_path, hashlib.sha256(manifest_bytes).hexdigest())


def _run(args, entries, owner, state_path, manifest_hash):
    root, out = args.root, args.out
    report = dict(schema=1, manifest=str(args.manifest), manifest_sha256=manifest_hash,
        scope=SCOPE, pixel_comparable=False, native_renderer=args.native_renderer,
        native_volumetrics=capture.NATIVE_VOLUMETRICS,
        requested_ids=[entry['id'] for entry in entries],
        results=[dict(id=entry['id'], status='pending', pixel_comparable=False,
                      limitations=entry.get('limitations', [])) for entry in entries])
    write_report(out, report)
    scenario = json.loads((root / 'scenario.json').read_text()) if state_path.exists() else None
    if scenario is not None:
        scenario.setdefault('native_renderer', 'wined3d')
    shared = None
    preparation_error = None
    implementations = {str(Path(module.__file__).resolve()): sha256(Path(module.__file__))
                       for module in (capture, parity, determinism, sys.modules[__name__])}
    for index, entry in enumerate(entries):
        row = report['results'][index]
        try:
            if preparation_error:
                raise RuntimeError(preparation_error)
            item, records = fixture_files(entry, root, out)
            if scenario is None:
                try:
                    prep = argparse.Namespace(**vars(args), item=item, properties=None)
                    scenario = capture.prepare(prep)
                    write_json(state_path, owner)
                except Exception as error:
                    preparation_error = f'Private preparation failed: {error}'
                    raise RuntimeError(preparation_error) from error
            if scenario.get('native_renderer') != args.native_renderer:
                raise ValueError('Prepared native renderer differs from requested renderer; use a new private root')
            capture.validate_native_quality(root, scenario)
            if shared is None:
                shared = shared_identity(root, scenario)
                if shared['adapter'] != scenario['adapter']['sha256']:
                    raise ValueError('Native adapter changed since preparation; use a new private root')
            settings = {key: getattr(args, key) for key in ('duration', 'warmup', 'startup_timeout')}
            identity = dict(fixture=entry, settings=dict(owner['settings'], **settings),
                            candidate=sha256(args.binary), candidate_path=str(args.binary),
                            shared=shared, implementations=implementations)
            key = hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest()
            directory = out / entry['id']
            latest = directory / 'latest.json'
            if args.resume and latest.exists():
                try:
                    previous = json.loads(latest.read_text())
                except (OSError, ValueError):
                    previous = None
                if can_resume(previous, key):
                    report['results'][index] = dict(previous, resumed=True)
                    write_report(out, report)
                    continue
            scenario = install_fixture(root, scenario, entry, item, records, manifest_hash)
            directory.mkdir(exist_ok=True)
            attempt = 1
            while (directory / f'attempt-{attempt:03d}').exists():
                attempt += 1
            run = directory / f'attempt-{attempt:03d}'
            row['run'] = str(run)
            pair = argparse.Namespace(**dict(vars(args), out=run, diagnostic=True, worker=None))
            comparison = capture.run_pair(pair, scenario)
            row = captured_result(row, run, comparison, key)
            write_json(latest, row)
        except Exception as error:
            row = dict(row, status='capture_failed', error=str(error))
        report['results'][index] = row
        write_report(out, report)
        print(f"{index + 1}/{len(entries)} {entry['id']}: {row['status']}", flush=True)
    write_report(out, report, sheets=True)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--adapter', type=Path, default=capture.DEFAULT_ADAPTER)
    parser.add_argument('--native-renderer', choices=['dxvk', 'wined3d'], default='dxvk',
                        help='Native D3D backend; prepared roots cannot change it (default: dxvk)')
    parser.add_argument('--fixture', action='append', default=[], help='Capture only this ID; repeat for multiple IDs')
    parser.add_argument('--resume', action='store_true', help='Reuse only content, runtime and artifact matched captures')
    parser.add_argument('--width', type=int, default=960)
    parser.add_argument('--height', type=int, default=540)
    parser.add_argument('--fps', type=int, default=15)
    parser.add_argument('--duration', type=float, default=1)
    parser.add_argument('--warmup', type=float, default=3)
    parser.add_argument('--startup-timeout', type=float, default=60)
    parser.add_argument('--post-processing', choices=['disabled', 'enabled', 'ultra'], default='enabled')
    args = parser.parse_args()
    if not (64 <= args.width <= 7680 and 64 <= args.height <= 4320 and 1 <= args.fps <= 240):
        parser.error('Invalid dimensions or FPS')
    if not (0 < args.duration <= 300 and 0 <= args.warmup <= 60 and 1 <= args.startup_timeout <= 300
            and round(args.duration * args.fps) >= 2):
        parser.error('Use at least 2 frames, duration (0,300], warmup [0,60], startup timeout [1,300]')
    report = run_sweep(args)
    return 1 if report['counts'].get('capture_failed') else 0


if __name__ == '__main__':
    sys.exit(main())
