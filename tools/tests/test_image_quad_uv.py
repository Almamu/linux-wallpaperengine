"""The scene-facing image quad insets UVs in source-storage texels."""
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest

from scene_render import render_scene


PROBE_VERTEX = '''
attribute vec3 a_Position;
attribute vec2 a_TexCoord;
uniform mat4 g_ModelViewProjectionMatrix;
varying vec2 v_TexCoord;
void main() {
    v_TexCoord = a_TexCoord;
    gl_Position = mul(vec4(a_Position, 1.0), g_ModelViewProjectionMatrix);
}
'''


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class ImageQuadUV(unittest.TestCase):
    def test_last_visible_effect_samples_the_scene_quad_after_each_toggle(self):
        from PIL import ImageChops

        with tempfile.TemporaryDirectory(prefix='lwe-dynamic-quad-') as directory:
            root = Path(directory)
            for folder in ('models', 'materials', 'shaders', 'effects'):
                (root / folder).mkdir()
            pixels = bytes([255]) * (8 * 8 * 4)
            (root / 'materials/source.tex').write_bytes(
                b'TEXV0005\0TEXI0001\0' + struct.pack('<7I', 0, 2, 8, 8, 8, 8, 0)
                + b'TEXB0001\0' + struct.pack('<5I', 1, 1, 8, 8, len(pixels)) + pixels)
            (root / 'models/probe.json').write_text(json.dumps({'material': 'materials/base.json'}))
            for name, textures, body in (
                ('base', ['source'], 'gl_FragColor = vec4(v_TexCoord, 0.4, 0.65);'),
                ('filter', [], '''
                    vec2 delta = v_TexCoord - vec2(0.5);
                    vec4 center = texSample2D(g_Texture0, v_TexCoord);
                    vec4 shifted = texSample2D(g_Texture0, v_TexCoord + 0.7 * delta * delta);
                    gl_FragColor = vec4(shifted.r, center.g, abs(ddx(v_TexCoord.x)) * 5.0, center.a);
                '''),
            ):
                (root / f'shaders/{name}.vert').write_text(PROBE_VERTEX)
                (root / f'shaders/{name}.frag').write_text(
                    'uniform sampler2D g_Texture0; varying vec2 v_TexCoord; '
                    'void main() { ' + body + ' }')
                (root / f'materials/{name}.json').write_text(json.dumps({'passes': [{
                    'shader': name, 'textures': textures, 'cullmode': 'nocull',
                    'blending': 'translucent' if name == 'base' else 'normal',
                    'depthtest': 'disabled', 'depthwrite': 'disabled'}]}))
            (root / 'effects/filter.json').write_text(json.dumps({
                'passes': [{'material': 'materials/filter.json'}]}))
            filter_effect = {'id': 4, 'file': 'effects/filter.json'}
            layer = {'name': 'Probe', 'image': 'models/probe.json', 'size': '64 64'}
            scene = {'camera': {'eye': '160 90 100', 'center': '160 90 0', 'up': '0 1 0'},
                     'general': {'orthogonalprojection': {'width': 320, 'height': 180},
                                 'clearcolor': '0.1 0.2 0.3', 'bloom': False, 'camerafade': False},
                     'objects': [
                         {**layer, 'id': 1, 'origin': '64 90 0', 'effects': [filter_effect]},
                         {**layer, 'id': 2, 'origin': '160 90 0', 'effects': [
                             {**filter_effect, 'visible': {'value': True, 'script':
                                 'let n = 0; export function update() { return ++n < 7 || n >= 10; }'}}]},
                         {**layer, 'id': 3, 'origin': '256 90 0'},
                     ]}
            for frames, expected_x in ((4, 64), (8, 256), (12, 64)):
                with self.subTest(frames=frames):
                    (root / 'frame.png').unlink(missing_ok=True)
                    image, _ = render_scene(self, root, scene, frames=frames)
                    actual = image.crop((130, 60, 190, 120))
                    expected = image.crop((expected_x - 30, 60, expected_x + 30, 120))
                    difference = ImageChops.difference(actual, expected)
                    self.assertLessEqual(max(high for _, high in difference.getextrema()), 1)

    def test_scene_quad_only_insets_eligible_images(self):
        with tempfile.TemporaryDirectory(prefix='lwe-image-quad-uv-') as directory:
            for name, model_flags, effect, sheet, sample, expected in (
                ('direct', {}, False, False, (128, 121), (6, 180, 0)),
                ('effect', {}, True, False, (128, 121), (7, 244, 16)),
                ('fullscreen', {'fullscreen': True}, False, False, (0, 179), (0, 191, 0)),
                ('passthrough', {'passthrough': True}, True, False, (128, 121), (2, 253)),
                ('solidlayer', {'solidlayer': True}, False, False, (128, 121), (1, 190, 0)),
                ('direct-nopadding', {'nopadding': True}, False, False, (128, 121), (1, 190, 0)),
                ('effect-nopadding', {'nopadding': True}, True, False, (128, 121), (7, 244, 16)),
                ('repeat-sheet', {}, False, True, (128, 121), (1, 190, 0)),
            ):
                with self.subTest(name=name):
                    root = Path(directory) / name
                    for folder in ('models', 'materials', 'shaders', 'effects'):
                        (root / folder).mkdir(parents=True, exist_ok=True)
                    # Six by three authored pixels in an eight by four allocation.
                    pixels = bytes([255]) * (8 * 4 * 4)
                    (root / 'materials/source.tex').write_bytes(
                        b'TEXV0005\0TEXI0001\0' + struct.pack('<7I', 0, 0 if sheet else 2, 8, 4, 6, 3, 0)
                        + b'TEXB0001\0' + struct.pack('<5I', 1, 1, 8, 4, len(pixels)) + pixels)
                    if sheet:
                        (root / 'materials/source.tex-json').write_text(json.dumps({
                            'spritesheetsequences': [{'frames': 2, 'width': 3, 'height': 3}]}))
                    (root / 'models/probe.json').write_text(json.dumps({
                        'material': 'materials/probe.json', **model_flags}))
                    for shader, textures, blue in (
                        ('probe', ['source'], '0.0'),
                        # Blue reads the first texel of the six-by-three local pass.
                        ('probe_effect', [], 'texSample2D(g_Texture0, vec2(1.0/12.0, 1.0/6.0)).r'),
                    ):
                        (root / f'shaders/{shader}.vert').write_text(PROBE_VERTEX)
                        (root / f'shaders/{shader}.frag').write_text(f'''
uniform sampler2D g_Texture0;
varying vec2 v_TexCoord;
void main() {{ gl_FragColor = vec4(v_TexCoord, {blue}, 1.0); }}
''')
                        (root / f'materials/{shader}.json').write_text(json.dumps({'passes': [{
                            'shader': shader, 'textures': textures, 'blending': 'normal',
                            'depthtest': 'disabled', 'depthwrite': 'disabled', 'cullmode': 'nocull'}]}))
                    layer = {'id': 1, 'name': name, 'image': 'models/probe.json',
                             'origin': '160 90 0', 'size': '64 64'}
                    if effect:
                        (root / 'effects/probe.json').write_text(json.dumps({
                            'passes': [{'material': 'materials/probe_effect.json'}]}))
                        layer['effects'] = [{'id': 2, 'file': 'effects/probe.json'}]
                    scene = {'camera': {'eye': '160 90 100', 'center': '160 90 0', 'up': '0 1 0'},
                             'general': {'orthogonalprojection': {'width': 320, 'height': 180},
                                         'clearcolor': '0 0 0', 'bloom': False, 'camerafade': False},
                             'objects': [layer]}
                    frame, _ = render_scene(self, root, scene)
                    actual = frame.getpixel(sample)
                    for value, want in zip(actual, expected):
                        self.assertAlmostEqual(value, want, delta=1, msg=f'{actual} != {expected}')


if __name__ == '__main__':
    unittest.main()
