"""Self-dependent image layers draw the final authored pass at local resolution."""
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest

from scene_render import render_scene
from test_image_quad_uv import PROBE_VERTEX


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class ImageSelfDependency(unittest.TestCase):
    def test_base_and_final_effect_use_local_pixels_for_self_dependency(self):
        with tempfile.TemporaryDirectory(prefix='lwe-self-dependency-') as directory:
            for with_effect in (False, True):
                with self.subTest(with_effect=with_effect):
                    root = Path(directory) / str(with_effect)
                    for folder in ('models', 'materials', 'shaders', 'effects'):
                        (root / folder).mkdir(parents=True)
                    rgba = bytes([255]) * (256 * 256 * 4)
                    (root / 'materials/source.tex').write_bytes(
                        b'TEXV0005\0TEXI0001\0' + struct.pack('<7I', 0, 2, 256, 256, 256, 256, 0)
                        + b'TEXB0001\0' + struct.pack('<5I', 1, 1, 256, 256, len(rgba)) + rgba)
                    for name, textures, expression in (
                        ('base', ['source'], 'vec4(footprint, g_Color4.ba)'),
                        ('effect', [], 'vec4(footprint, texSample2D(g_Texture0, v_TexCoord).ba)'),
                        ('export', ['_rt_imageLayerComposite_1_a'], 'texSample2D(g_Texture0, v_TexCoord)'),
                    ):
                        (root / f'shaders/{name}.vert').write_text(PROBE_VERTEX)
                        (root / f'shaders/{name}.frag').write_text(f'''
uniform vec4 g_Color4;
uniform sampler2D g_Texture0;
varying vec2 v_TexCoord;
void main() {{
    vec2 footprint = vec2(abs(ddx(v_TexCoord.x)), abs(ddy(v_TexCoord.y))) * 32.0;
    gl_FragColor = {expression};
}}
''')
                        (root / f'materials/{name}.json').write_text(json.dumps({'passes': [{
                            'shader': name, 'textures': textures, 'cullmode': 'nocull',
                            'blending': 'translucent' if name == 'base' else 'normal',
                            'depthtest': 'disabled', 'depthwrite': 'disabled'}]}))
                        (root / f'models/{name}.json').write_text(json.dumps({'material': f'materials/{name}.json'}))
                    (root / 'effects/probe.json').write_text(json.dumps({
                        'passes': [{'material': 'materials/effect.json'}]}))
                    base = {'name': 'Footprint', 'image': 'models/base.json', 'size': '64 64',
                            'color': '1 1 0.6', 'alpha': 0.5}
                    if with_effect:
                        base['effects'] = [{'id': 4, 'file': 'effects/probe.json'}]
                    scene = {'camera': {'eye': '160 90 100', 'center': '160 90 0', 'up': '0 1 0'},
                             'general': {'orthogonalprojection': {'width': 320, 'height': 180},
                                         'clearcolor': '0 0 0', 'bloom': False, 'camerafade': False},
                             'objects': [
                                 {**base, 'id': 1, 'origin': '64 90 0', 'dependencies': [1]},
                                 {**base, 'id': 2, 'origin': '160 90 0'},
                                 {'id': 3, 'name': 'Export', 'image': 'models/export.json',
                                  'origin': '256 90 0', 'size': '64 64', 'dependencies': [1]},
                             ]}
                    frame, _ = render_scene(self, root, scene)
                    for x, expected in ((64, (16, 16, 77)), (160, (64, 64, 77)), (256, (32, 32, 153))):
                        actual = frame.getpixel((x, 90))
                        for value, want in zip(actual, expected):
                            self.assertAlmostEqual(value, want, delta=1, msg=f'x={x}: {actual} != {expected}')


if __name__ == '__main__':
    unittest.main()
