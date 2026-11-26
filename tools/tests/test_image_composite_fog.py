"""Native image fog belongs to modern scene presentation, never exported textures."""
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest

from scene_render import render_scene


COPY_VERTEX = '''
attribute vec3 a_Position;
attribute vec2 a_TexCoord;
uniform mat4 g_ModelViewProjectionMatrix;
varying vec2 v_TexCoord;
void main() {
    v_TexCoord = a_TexCoord;
    gl_Position = mul(vec4(a_Position, 1.0), g_ModelViewProjectionMatrix);
}
'''


def write_model(root):
    """A skinned triangle with the fixed Idle/Action clips used by the probes."""
    data = bytearray()
    def put(fmt, *values):
        data.extend(struct.pack('<' + fmt, *values))
    def string(value):
        data.extend(value.encode() + b'\0')
    def begin(marker):
        string(marker)
        offset = len(data)
        put('I', 0)
        return offset
    def end(offset):
        struct.pack_into('<I', data, offset, len(data))
    identity = [1 if row == column else 0 for column in range(4) for row in range(4)]

    string('MDLV0023')
    put('III', 0x0180000f, 1, 1)
    string('materials/probe.json')
    put('I6fII', 0, -5, -5, 0, 5, 5, 0, 0x0180000f, 3 * 80)
    for x, y in ((-5, -5), (5, -5), (0, 5)):
        put('10f4I6f', x, y, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0)
    put('I3HBBI', 6, 0, 1, 2, 0, 0, 0)
    offset = begin('MDLS0002')
    put('I', 1)
    string('root')
    put('III', 0, 0xffffffff, 64)
    put('16f', *identity)
    string('')
    put('HBIHHBB', 0, 0, 0, 0, 0, 0, 0)
    end(offset)
    offset = begin('MDAT0001')
    put('HH', 1, 0)
    string('tip')
    put('16f', *identity)
    end(offset)
    offset = begin('MDLA0006')
    put('I', 2)
    for clip_id, name, base, events in (
            (1, 'Idle', 0, [(9, 'end')]), (2, 'Action', 100, [(3, 'cry'), (9, 'reset')])):
        put('II', clip_id, 0)
        string(name)
        string('loop')
        put('fIII', 10, 10, 0, 1)
        put('II', 0, 11 * 36)
        for frame in range(11):
            put('9f', base + frame, 0, 0, 0, 0, 0, 1, 1, 1)
        put('IBB', 0, 0, 0)
        put('6f', *([0] * 6))
        put('BI', 0, len(events))
        for frame, event in events:
            put('f', frame / 10)
            string(json.dumps({'frame': frame, 'name': event}))
    end(offset)
    (root / 'models').mkdir(parents=True)
    (root / 'models/probe.mdl').write_bytes(data)
    (root / 'materials').mkdir()
    (root / 'materials/probe.json').write_text(json.dumps({'passes': [{
        'shader': 'generic4', 'textures': ['util/white'], 'blending': 'normal',
        'cullmode': 'nocull', 'depthtest': 'enabled', 'depthwrite': 'enabled'}]}))


def fog_scene(root, *, composite, visible, effect, lighting=False, fog=True, computed=None,
              model=False, solid=False, perspective=False, version=None, distance=10):
    for folder in ('models', 'materials'):
        (root / folder).mkdir(parents=True, exist_ok=True)
    for name, texture, combos in (
            ('source', 'util/white', {'LIGHTING': int(lighting), 'REFLECTION': 0}),
            ('consumer', '_rt_imageLayerComposite_10_a', {'FOG': 0})):
        if name == 'source':
            if fog is not None:
                combos['FOG'] = int(fog)
            if computed is not None:
                combos['FOG_COMPUTED'] = int(computed)
        (root / f'models/{name}.json').write_text(json.dumps({
            'material': f'materials/{name}.json', 'solidlayer': solid}))
        (root / f'materials/{name}.json').write_text(json.dumps({'passes': [{
            'shader': 'generic4' if model and name == 'source' else 'genericimage4',
            'textures': [texture], 'combos': combos,
            'blending': 'normal', 'depthtest': 'disabled', 'depthwrite': 'disabled', 'cullmode': 'nocull'}]}))
    source = {'id': 10, 'name': 'source', 'image': 'models/source.json', 'origin': '0 0 0',
              'size': '256 256', 'scale': '0.015625 0.015625 0.015625', 'visible': visible,
              'perspective': perspective}
    if model:
        vertices = b''.join(struct.pack('<12f', x, y, 0, 0, 0, 1, 1, 0, 0, 1, u, v)
                            for x, y, u, v in ((-2, -2, 0, 1), (2, -2, 1, 1),
                                               (2, 2, 1, 0), (-2, 2, 0, 0)))
        indices = struct.pack('<6H', 0, 1, 2, 0, 2, 3)
        (root / 'models/source.mdl').write_bytes(
            b'MDLV0016\0' + struct.pack('<3I', 15, 1, 1) + b'materials/source.json\0'
            + struct.pack('<3I', 0, 15, len(vertices)) + vertices
            + struct.pack('<I', len(indices)) + indices + b'\0')
        source = {'id': 10, 'name': 'model fog control', 'model': 'models/source.mdl',
                  'origin': '0 0 0'}
    if effect:
        source['effects'] = [{'id': 40, 'file': 'effects/scroll/effect.json', 'passes': [
            {'constantshadervalues': {'speedx': 0, 'speedy': 0}}]}]
    objects = [source]
    if composite:
        objects.append({'id': 11, 'name': 'consumer', 'image': 'models/consumer.json',
                        'origin': '0 0 0', 'size': '4 4', 'dependencies': [10]})
    objects.append({'id': 99, 'name': 'camera', 'camera': 'default', 'origin': f'0 0 {distance}',
                    'angles': '0 0 0', 'fov': 50})
    scene = {'camera': {'eye': f'0 0 {distance}', 'center': '0 0 0', 'up': '0 1 0'},
            'general': {'orthogonalprojection': None, 'nearz': .1, 'farz': 100, 'fov': 50,
                        'clearcolor': '0 0 0', 'camerafade': False, 'bloom': False, 'hdr': False,
                        'ambientcolor': '1 1 1', 'skylightcolor': '0 0 0',
                        'fogdistance': True, 'fogdistancecolor': '0 1 0',
                        'fogdistancestart': 0, 'fogdistanceend': 1,
                        'fogdistancestartdensity': 1, 'fogdistanceenddensity': 1},
            'objects': objects}
    if version is not None:
        scene['version'] = version
    return scene


def image_export_time_scene(root, *, sorting):
    """Expose a hidden image's export age through an opaque model consumer."""
    scene = fog_scene(root, composite=True, visible=False, effect=False,
                      fog=False, version=5, distance=5)
    scene['general'].update(fogdistance=False, transparentsorting=sorting)
    (root / 'shaders').mkdir()
    (root / 'effects').mkdir()
    # Keep the native control independent of fork-only stock effect overrides.
    (root / 'shaders/identity.vert').write_text(COPY_VERTEX)
    (root / 'shaders/identity.frag').write_text('''
uniform sampler2D g_Texture0;
varying vec2 v_TexCoord;
void main() { gl_FragColor = texSample2D(g_Texture0, v_TexCoord); }
''')
    (root / 'materials/identity.json').write_text(json.dumps({'passes': [{
        'shader': 'identity', 'blending': 'normal', 'depthtest': 'disabled',
        'depthwrite': 'disabled', 'cullmode': 'nocull'}]}))
    (root / 'effects/identity.json').write_text(json.dumps({'passes': [{
        'material': 'materials/identity.json'}]}))
    scene['objects'][0]['effects'] = [{'id': 40, 'file': 'effects/identity.json'}]
    signal = 'vec3(0.25 + frac(g_Time * 7.0) * 0.5, 0.25 + frac(g_Time * 11.0) * 0.5, 0.625)'
    for name, body in (
            ('source', f'gl_FragColor = vec4({signal}, 1.0);'),
            ('consumer', f'''
    vec3 sampled = texSample2D(g_Texture0, vec2(0.5, 0.5)).rgb;
    vec3 expected = {signal};
    vec3 error = abs(sampled - expected);
    bool current = max(error.r, max(error.g, error.b)) <= 2.0 / 255.0;
    vec3 result = current ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    if (v_TexCoord.x < 0.333) result = sampled;
    if (v_TexCoord.x > 0.667) result = expected;
    gl_FragColor = vec4(result, 1.0);
''')):
        (root / f'shaders/{name}.vert').write_text(COPY_VERTEX)
        (root / f'shaders/{name}.frag').write_text('''
uniform float g_Time;
uniform sampler2D g_Texture0;
varying vec2 v_TexCoord;
void main() { BODY }
'''.replace('BODY', body))
        path = root / f'materials/{name}.json'
        material = json.loads(path.read_text())
        material['passes'][0].update(shader=name, blending=(
            'translucent' if name == 'source' else 'alphatocoverage'))
        path.write_text(json.dumps(material))
    vertices = b''.join(struct.pack('<12f', x, y, 0, 0, 0, 1, 1, 0, 0, 1, u, v)
                        for x, y, u, v in ((-2, -2, 0, 1), (2, -2, 1, 1),
                                           (2, 2, 1, 0), (-2, 2, 0, 0)))
    (root / 'models/consumer.mdl').write_bytes(
        b'MDLV0023\0' + struct.pack('<3I', 15, 1, 1) + b'materials/consumer.json\0'
        + struct.pack('<I6fII', 0, -2, -2, 0, 2, 2, 0, 15, len(vertices)) + vertices
        + struct.pack('<I6HBBI', 12, 0, 1, 2, 0, 2, 3, 0, 0, 0))
    source, _, camera = scene['objects']
    # Native source-first controls use the current export without sorting.
    # Automatic sorting moves the opaque model before this translucent source,
    # so it reads the retained export before the image updates later that frame.
    scene['objects'] = [
        source,
        {'id': 11, 'name': 'export age diagnostic', 'model': 'models/consumer.mdl',
         'dependencies': [10]}, camera]
    return scene


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class ImageCompositeFog(unittest.TestCase):
    def test_sorted_models_keep_previous_frame_hidden_image_exports(self):
        # The center reports the age in the shader against that same frame's
        # g_Time; the side bands expose sampled/expected RGB for native probes.
        # Two different ramp rates expose a previous-frame sample even when a
        # native control renders faster than the fork's requested 10 fps.
        # Native Deep Sea Dive captures show main/shadow model reads before
        # the only image export write; source-first native controls distinguish
        # that sorted timing from current-frame unsorted traversal.
        for sorting in (False, True):
            for frames in (5, 20):
                with self.subTest(sorting=sorting, frames=frames), tempfile.TemporaryDirectory(prefix='lwe-export-time-') as directory:
                    root = Path(directory)
                    frame, output = render_scene(self, root, image_export_time_scene(root, sorting=sorting),
                        frames=frames, fps=10,
                        extra_args=('--msaa', 'off', '--post-processing', 'disabled'))
                    self.assertNotIn('Failed to setup', output)
                    actual = frame.getpixel((160, 90))
                    sampled, expected = frame.getpixel((110, 90)), frame.getpixel((210, 90))
                    diagnostic = f'Frame {frames}, sorting={sorting}: sampled={sampled}, expected={expected}, diagnostic={actual}'
                    self.assertAlmostEqual(sampled[2], 159, delta=1, msg=diagnostic)
                    self.assertEqual(actual, (255, 0, 0) if sorting else (0, 255, 0), diagnostic)
                    error = max(abs(a - b) for a, b in zip(sampled, expected))
                    if sorting:
                        self.assertGreater(error, 2, diagnostic)
                    else:
                        self.assertLessEqual(error, 2, diagnostic)

    def test_fog_presentation_preserves_lit_image_and_puppet_pixels(self):
        # Zero fog density must preserve point lighting when the additional
        # scene-final pass moves the source material into an offscreen target.
        for puppet in ('none', 'static', 'deformed'):
            with self.subTest(puppet=puppet), tempfile.TemporaryDirectory(prefix='lwe-lit-fog-') as directory:
                pixels = []
                for enabled in (False, True):
                    root = Path(directory) / str(enabled)
                    if puppet != 'none':
                        write_model(root)
                        mdl = root / 'models/probe.mdl'
                        data = bytearray(mdl.read_bytes())
                        if puppet == 'static':
                            data = data.split(b'MDLA0006\0')[0]
                        start = data.index(b'materials/probe.json\0') + len(b'materials/probe.json\0') + 36
                        for index, uv in enumerate(((0, 1), (1, 1), (.5, 0))):
                            struct.pack_into('<2f', data, start + index * 80 + 72, *uv)
                        mdl.write_bytes(data)
                    scene = fog_scene(root, composite=False, visible=True, effect=False,
                                      lighting=True, version=5, distance=5)
                    scene['objects'][0]['origin'] = '.8 0 0'
                    if puppet != 'none':
                        path = root / 'models/source.json'
                        model = json.loads(path.read_text())
                        model.update(puppet='models/probe.mdl', width=10, height=10)
                        path.write_text(json.dumps(model))
                        scene['objects'][0].update(size='10 10', scale='.4 .4 .4')
                    if puppet == 'deformed':
                        scene['objects'][0]['animationlayers'] = [{
                            'id': 10, 'name': 'Idle', 'animation': 1,
                            'blend': {'value': 1, 'script': '''export function init(value) {
    const clip = thisLayer.getAnimationLayer('Idle');
    clip.pause();
    clip.setFrame(2);
    return value;
}'''}}]
                    scene['general'].update(fogdistance=enabled, fogdistancestartdensity=0,
                                            fogdistanceenddensity=0, ambientcolor='0 0 0')
                    light_x = 1.6 if puppet == 'deformed' else .8
                    scene['objects'].append({'id': 100, 'light': 'point', 'origin': f'{light_x} 0 1',
                                             'radius': 2, 'intensity': 2, 'color': '1 1 1'})
                    frame, _ = render_scene(self, root, scene)
                    pixels.append(frame.getpixel((222 if puppet == 'deformed' else 191, 90)))
                self.assertGreater(min(pixels[0]), 20, pixels)
                self.assertLessEqual(max(abs(a - b) for a, b in zip(*pixels)), 3, pixels)

    def test_modern_exports_include_the_last_effect_without_scene_fog(self):
        for visible in (False, True):
            with self.subTest(visible=visible), tempfile.TemporaryDirectory(prefix='lwe-fog-export-') as directory:
                root = Path(directory)
                scene = fog_scene(root, composite=True, visible=visible, effect=False, version=5)
                scene['objects'][0]['effects'] = [{'id': 40, 'file': 'effects/tint/effect.json',
                    'passes': [{'combos': {'BLENDMODE': 0}, 'constantshadervalues': {
                        'color': '1 0 0', 'alpha': 1}}]}]
                frame, _ = render_scene(self, root, scene)
                actual = frame.getpixel((160, 90))
                self.assertLessEqual(max(abs(a - b) for a, b in zip(actual, (255, 0, 0))), 1, actual)

    def test_puppet_prelighting_does_not_reuse_previous_effect_pixels(self):
        # The rest mesh covers only half the image target. This ordinary
        # brightness effect also writes outside that triangle. Exporting its
        # output back to _a must not turn those pixels into next frame's input.
        samples = []
        for frames in (3, 20):
            with self.subTest(frames=frames), tempfile.TemporaryDirectory(prefix='lwe-puppet-fog-feedback-') as directory:
                root = Path(directory)
                write_model(root)
                for folder in ('shaders', 'effects'):
                    (root / folder).mkdir()
                mdl = root / 'models/probe.mdl'
                data = bytearray(mdl.read_bytes())
                start = data.index(b'materials/probe.json\0') + len(b'materials/probe.json\0') + 36
                for index, uv in enumerate(((0, 1), (1, 1), (.5, 0))):
                    struct.pack_into('<2f', data, start + index * 80 + 72, *uv)
                mdl.write_bytes(data)
                scene = fog_scene(root, composite=True, visible=True, effect=False,
                                  lighting=True, version=5, distance=5)
                scene['general'].update(fogdistancestartdensity=0, fogdistanceenddensity=0)
                model_path = root / 'models/source.json'
                model = json.loads(model_path.read_text())
                model.update(puppet='models/probe.mdl', width=10, height=10)
                model_path.write_text(json.dumps(model))
                scene['objects'][0].update(origin='.8 0 0', size='10 10', scale='.4 .4 .4',
                    animationlayers=[{'id': 10, 'name': 'Idle', 'animation': 1,
                        'blend': {'value': 1, 'script': '''export function init(value) {
    const clip = thisLayer.getAnimationLayer('Idle');
    clip.pause();
    clip.setFrame(2);
    return value;
}'''}}], effects=[{'id': 40, 'file': 'effects/brightness.json'}])
                for name, expression in (
                        ('brightness', 'vec4(texSample2D(g_Texture0, v_TexCoord).rgb + vec3(0.125, 0.0, 0.0), 1.0)'),
                        ('exportprobe', 'vec4(texSample2D(g_Texture0, v_TexCoord.x < 0.5 ? vec2(0.1, 0.1) : vec2(0.5, 0.75)).rgb, 1.0)')):
                    (root / f'shaders/{name}.vert').write_text(COPY_VERTEX)
                    (root / f'shaders/{name}.frag').write_text('''
uniform sampler2D g_Texture0;
varying vec2 v_TexCoord;
void main() { gl_FragColor = EXPRESSION; }
'''.replace('EXPRESSION', expression))
                (root / 'materials/brightness.json').write_text(json.dumps({'passes': [{
                    'shader': 'brightness', 'blending': 'normal', 'depthtest': 'disabled',
                    'depthwrite': 'disabled', 'cullmode': 'nocull'}]}))
                (root / 'effects/brightness.json').write_text(json.dumps({'passes': [{
                    'material': 'materials/brightness.json'}]}))
                consumer_path = root / 'materials/consumer.json'
                consumer = json.loads(consumer_path.read_text())
                consumer['passes'][0]['shader'] = 'exportprobe'
                consumer_path.write_text(json.dumps(consumer))
                frame, _ = render_scene(self, root, scene, frames=frames)
                outside, inside = frame.getpixel((120, 90)), frame.getpixel((200, 90))
                self.assertGreater(min(inside), 200, f'Missing lit puppet coverage: {inside}')
                samples.append(outside)
                self.assertLessEqual(max(abs(a - b) for a, b in zip(outside, (32, 0, 0))), 1,
                                     f'Frame {frames}: stale effect pixels outside rest triangle: {outside}')
        self.assertEqual(samples[0], samples[1])

    def test_modern_scene_fog_is_applied_only_when_presented(self):
        # Native 140206ae0 selects the fog-capable final material only after
        # scene version 2. Hidden consumers sample the image before that pass.
        for name, settings, expected in (
                ('legacy-v2', dict(version=2), (255, 255, 255)),
                ('modern-v3', dict(version=3), (0, 255, 0)),
                ('modern-default-fog', dict(fog=None), (0, 255, 0)),
                ('modern-effect', dict(effect=True), (0, 255, 0)),
                ('modern-lit-direct', dict(lighting=True), (0, 255, 0)),
                ('modern-lit-effect', dict(effect=True, lighting=True), (0, 255, 0)),
                ('modern-opt-out', dict(fog=False), (255, 255, 255)),
                ('modern-effect-opt-out', dict(effect=True, fog=False), (255, 255, 255)),
                ('modern-computed-off', dict(computed=False), (0, 255, 0)),
                ('modern-hidden-computed-on', dict(composite=True, visible=False, computed=True), (255, 255, 255)),
                ('modern-visible-computed-on', dict(composite=True, computed=True), (255, 255, 255)),
                ('modern-hidden-export', dict(composite=True, visible=False), (255, 255, 255)),
                ('modern-hidden-effect-export', dict(composite=True, visible=False, effect=True), (255, 255, 255)),
                ('modern-visible-effect-export', dict(composite=True, effect=True), (255, 255, 255))):
            with self.subTest(name=name), tempfile.TemporaryDirectory(prefix='lwe-modern-fog-') as directory:
                root = Path(directory)
                options = dict(composite=False, visible=True, effect=False, version=5)
                options.update(settings)
                frame, _ = render_scene(self, root, fog_scene(root, **options))
                actual = frame.getpixel((160, 90))
                self.assertLessEqual(max(abs(a - b) for a, b in zip(actual, expected)), 1,
                                     f'{name}: expected {expected}, got {actual}')

    def test_modern_fog_preserves_alpha_cutouts_without_msaa(self):
        for alpha, expected in ((0.25, (0, 0, 0)), (1, (0, 255, 0))):
            with self.subTest(alpha=alpha), tempfile.TemporaryDirectory(prefix='lwe-fog-cutout-') as directory:
                root = Path(directory)
                scene = fog_scene(root, composite=False, visible=True, effect=False, version=5)
                scene['objects'][0]['alpha'] = alpha
                path = root / 'materials/source.json'
                material = json.loads(path.read_text())
                material['passes'][0]['blending'] = 'alphatocoverage'
                path.write_text(json.dumps(material))
                frame, _ = render_scene(self, root, scene, extra_args=('--msaa', 'off'))
                actual = frame.getpixel((160, 90))
                self.assertLessEqual(max(abs(a - b) for a, b in zip(actual, expected)), 1,
                                     f'Expected {expected}, got {actual}')

    def test_modern_plain_and_effected_images_follow_world_distance(self):
        # Five independent camera distances bracket the native squared ramp.
        # Applying fog in image/effect coordinates or twice changes these RGBs.
        for effect in (False, True):
            for distance, expected in ((0.5, (0, 0, 255)), (1, (0, 0, 255)),
                                       (3, (64, 0, 191)), (5, (255, 0, 0)), (7, (255, 0, 0))):
                with self.subTest(effect=effect, distance=distance), tempfile.TemporaryDirectory(prefix='lwe-fog-distance-') as directory:
                    root = Path(directory)
                    scene = fog_scene(root, composite=False, visible=True, effect=effect,
                                      fog=None, version=5, distance=distance)
                    scene['objects'][0]['color'] = '0 0 1'
                    scene['general'].update(fogdistancecolor='1 0 0', fogdistancestart=1,
                                            fogdistanceend=5, fogdistancestartdensity=0)
                    frame, _ = render_scene(self, root, scene)
                    actual = frame.getpixel((160, 90))
                    self.assertLessEqual(max(abs(a - b) for a, b in zip(actual, expected)), 1,
                                         f'effect={effect}, distance={distance}: expected {expected}, got {actual}')

    def test_image_fog_requires_the_separate_computed_combo(self):
        # Full green distance fog makes any accidental bake into the white
        # exported texture visible through a consumer whose material opts out.
        for name, settings, expected in (
                ('hidden', dict(composite=True, visible=False, effect=True), (255, 255, 255)),
                ('hidden-default-fog', dict(composite=True, visible=False, effect=True, fog=None), (255, 255, 255)),
                ('visible', dict(composite=True, visible=True, effect=True), (255, 255, 255)),
                ('lit-hidden', dict(composite=True, visible=False, effect=True, lighting=True), (255, 255, 255)),
                ('direct', dict(composite=False, visible=True, effect=False), (255, 255, 255)),
                ('effect', dict(composite=False, visible=True, effect=True), (255, 255, 255)),
                ('lit-effect', dict(composite=False, visible=True, effect=True, lighting=True), (255, 255, 255)),
                ('solid-effect', dict(composite=False, visible=True, effect=True, solid=True), (255, 255, 255)),
                ('perspective-effect', dict(composite=False, visible=True, effect=True, perspective=True), (255, 255, 255)),
                ('effect-opt-out', dict(composite=False, visible=True, effect=True, fog=False), (255, 255, 255)),
                ('explicit-computed', dict(composite=False, visible=True, effect=False, computed=True), (0, 255, 0)),
                ('explicit-computed-off', dict(composite=False, visible=True, effect=False, computed=False), (255, 255, 255)),
                ('model', dict(composite=False, visible=True, effect=False, model=True), (0, 255, 0))):
            with self.subTest(name=name), tempfile.TemporaryDirectory(prefix='lwe-composite-fog-') as directory:
                root = Path(directory)
                frame, _ = render_scene(self, root, fog_scene(root, **settings))
                actual = frame.getpixel((160, 90))
                self.assertLessEqual(max(abs(a - b) for a, b in zip(actual, expected)), 1,
                                     f'{name}: expected {expected}, got {actual}')


if __name__ == '__main__':
    unittest.main()
