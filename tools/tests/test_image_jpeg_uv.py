"""A compiled JPEG's direct scene quad measures the inset in base-mip texels."""
from io import BytesIO
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest

from scene_render import render_scene


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class ImageJpegUV(unittest.TestCase):
    def test_padded_jpeg_uses_base_mip_extent_even_at_half_quality(self):
        from PIL import Image

        with tempfile.TemporaryDirectory(prefix='lwe-image-jpeg-uv-') as directory:
            root = Path(directory)
            for folder in ('models', 'materials', 'shaders'):
                (root / folder).mkdir()
            # Authoring retains a 16x8 TEXI store while the base JPEG fills only 8x4.
            packed = (b'TEXV0005\0TEXI0001\0' + struct.pack('<7I', 0, 2, 16, 8, 8, 4, 0)
                      + b'TEXB0003\0' + struct.pack('<3I', 1, 2, 2))
            for width, height, level in [(8, 4, 63), (4, 2, 191)]:
                encoded = BytesIO()
                Image.new('RGB', (width, height), (level,) * 3).save(encoded, 'JPEG')
                pixels = encoded.getvalue()
                packed += struct.pack('<5I', width, height, 0, len(pixels), len(pixels)) + pixels
            (root / 'materials/source.tex').write_bytes(packed)
            (root / 'models/probe.json').write_text(json.dumps({'material': 'materials/probe.json'}))
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
            scene = {'camera': {'eye': '160 90 100', 'center': '160 90 0', 'up': '0 1 0'},
                     'general': {'orthogonalprojection': {'width': 320, 'height': 180},
                                 'clearcolor': '0 0 0', 'bloom': False, 'camerafade': False},
                     'objects': [{'id': 1, 'name': 'Probe', 'image': 'models/probe.json',
                                  'origin': '160 90 0', 'size': '64 64'}]}
            for quality, marker in [('full', 63), ('half', 191)]:
                with self.subTest(quality=quality):
                    (root / 'frame.png').unlink(missing_ok=True)
                    frame, _ = render_scene(self, root, scene, extra_args=('--texture-resolution', quality))
                    red, green, blue = frame.getpixel((128, 121))
                    # This pixel center is half a pixel from the bottom-left corner of the
                    # 64x64 quad. At both qualities, only blue follows the selected mip.
                    u = 0.5 / 64 + (0.15 / 8) * (1 - 1 / 64)
                    v = 1 - 0.5 / 64 - (0.15 / 4) * (1 - 1 / 64)
                    self.assertAlmostEqual(blue, marker, delta=1)
                    self.assertAlmostEqual(red, round(255 * u), delta=1)
                    self.assertAlmostEqual(green, round(255 * v), delta=1)


if __name__ == '__main__':
    unittest.main()
