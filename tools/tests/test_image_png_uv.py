"""Ordinary compiled PNG scene quads measure their inset in base-mip texels."""
from io import BytesIO
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest

from scene_render import render_scene


def write_probe(root, storage, nopadding=False):
    from PIL import Image

    for folder in ('models', 'materials', 'shaders'):
        (root / folder).mkdir(parents=True)
    packed = (b'TEXV0005\0TEXI0001\0' + struct.pack('<7I', 0, 2, *storage, 8, 4, 0)
              + b'TEXB0003\0' + struct.pack('<3I', 1, 13, 2))
    for width, height, marker in ((8, 4, 63), (4, 2, 191)):
        encoded = BytesIO()
        Image.new('RGB', (width, height), (marker,) * 3).save(encoded, 'PNG')
        pixels = encoded.getvalue()
        packed += struct.pack('<5I', width, height, 0, len(pixels), len(pixels)) + pixels
    (root / 'materials/source.tex').write_bytes(packed)
    (root / 'models/probe.json').write_text(json.dumps({'material': 'materials/probe.json', 'nopadding': nopadding}))
    (root / 'materials/probe.json').write_text(json.dumps({'passes': [{
        'shader': 'probe', 'textures': ['source'], 'blending': 'normal',
        'depthtest': 'disabled', 'depthwrite': 'disabled', 'cullmode': 'nocull'}]}))
    (root / 'shaders/probe.vert').write_text('''
attribute vec3 a_Position;
attribute vec2 a_TexCoord;
uniform mat4 g_ModelViewProjectionMatrix;
varying vec2 v_TexCoord;
void main() {
    v_TexCoord = a_TexCoord;
    gl_Position = mul(vec4(a_Position, 1.0), g_ModelViewProjectionMatrix);
}
''')
    (root / 'shaders/probe.frag').write_text('''
uniform sampler2D g_Texture0;
varying vec2 v_TexCoord;
void main() {
    gl_FragColor = vec4(v_TexCoord, texSample2D(g_Texture0, vec2(0.5)).b, 1.0);
}
''')
    return {'camera': {'eye': '160 90 100', 'center': '160 90 0', 'up': '0 1 0'},
            'general': {'orthogonalprojection': {'width': 320, 'height': 180},
                        'clearcolor': '0 0 0', 'bloom': False, 'camerafade': False},
            'objects': [{'id': 1, 'name': 'Probe', 'image': 'models/probe.json',
                         'origin': '160 90 0', 'size': '64 64'}]}


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class ImagePngUV(unittest.TestCase):
    def test_padded_and_unpadded_png_insets(self):
        for name, storage, nopadding, quality, marker in (
                ('padded-full', (16, 8), False, 'full', 63),
                ('padded-half', (16, 8), False, 'half', 191),
                ('unpadded', (8, 4), False, 'full', 63),
                ('nopadding', (16, 8), True, 'full', 63)):
            with self.subTest(name=name), tempfile.TemporaryDirectory(prefix=f'lwe-image-png-{name}-') as directory:
                root = Path(directory)
                scene = write_probe(root, storage, nopadding)
                frame, _ = render_scene(self, root, scene, extra_args=('--texture-resolution', quality))
                red, green, blue = frame.getpixel((128, 121))
                inset = 0 if nopadding else .15
                u = .5/64 + inset/8*(1-1/64)
                v = 1-.5/64 - inset/4*(1-1/64)
                self.assertAlmostEqual(blue, marker, delta=1)
                self.assertAlmostEqual(red, round(255*u), delta=1)
                self.assertAlmostEqual(green, round(255*v), delta=1)


if __name__ == '__main__':
    unittest.main()
