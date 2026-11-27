"""Image-bound static MDL geometry draws its authored indices and UVs."""

import json
import os
from pathlib import Path
import struct
import tempfile
import unittest

from scene_render import render_scene


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class PuppetStaticMesh(unittest.TestCase):
    def test_image_draws_unskinned_triangle_with_authored_uvs(self):
        with tempfile.TemporaryDirectory(prefix='lwe-puppet-static-') as directory:
            root = Path(directory)
            for folder in ('models', 'materials', 'shaders'):
                (root / folder).mkdir()

            # MDLV0023 layout 0x0f: position, normal, tangent and UV in 48 bytes.
            mesh = b'MDLV0023\0' + struct.pack('<3I', 0xf, 1, 1) + b'materials/probe.json\0'
            mesh += struct.pack('<I6fII', 0, -24, -24, 0, 24, 24, 0, 0xf, 3 * 48)
            for x, y in ((-24, -24), (24, -24), (-24, 24)):
                mesh += struct.pack('<12f', x, y, 0, 0, 0, 1, 1, 0, 0, 1, 0.25, 0.75)
            mesh += struct.pack('<I3HBBI', 6, 0, 1, 2, 0, 0, 0)
            (root / 'models/probe.mdl').write_bytes(mesh)
            (root / 'models/probe.json').write_text(json.dumps({
                'material': 'materials/probe.json', 'puppet': 'models/probe.mdl'}))
            (root / 'materials/probe.json').write_text(json.dumps({'passes': [{
                'shader': 'probe', 'blending': 'normal', 'cullmode': 'nocull',
                'depthtest': 'disabled', 'depthwrite': 'disabled'}]}))
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
varying vec2 v_TexCoord;
void main() { gl_FragColor = vec4(v_TexCoord, 0.5, 1.0); }
''')
            scene = {
                'camera': {'eye': '160 90 100', 'center': '160 90 0', 'up': '0 1 0'},
                'general': {'orthogonalprojection': {'width': 320, 'height': 180},
                            'clearcolor': '0 0 0', 'bloom': False, 'camerafade': False},
                'objects': [{'id': 1, 'name': 'Triangle', 'image': 'models/probe.json',
                             'origin': '160 90 0', 'size': '64 64'}],
            }
            frame, output = render_scene(self, root, scene)
            self.assertEqual(frame.getpixel((176, 106)), (0, 0, 0), output)
            for actual, expected in zip(frame.getpixel((144, 106)), (64, 191, 128)):
                self.assertAlmostEqual(actual, expected, delta=1, msg=output)


if __name__ == '__main__':
    unittest.main()
