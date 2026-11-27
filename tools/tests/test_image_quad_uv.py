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
