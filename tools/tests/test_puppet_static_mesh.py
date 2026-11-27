"""Image-bound static MDL geometry draws its authored indices and UVs."""

import json
import os
from pathlib import Path
import struct
import tempfile
import unittest

from scene_render import render_scene


def triangle(material, uv_coordinates):
    # MDLV0023 layout 0x0f: position, normal, tangent and UV in 48 bytes.
    mesh = b'MDLV0023\0' + struct.pack('<3I', 0xf, 1, 1) + material.encode() + b'\0'
    mesh += struct.pack('<I6fII', 0, -24, -24, 0, 24, 24, 0, 0xf, 3 * 48)
    for (x, y), (u, v) in zip(((-24, -24), (24, -24), (-24, 24)), uv_coordinates):
        mesh += struct.pack('<12f', x, y, 0, 0, 0, 1, 1, 0, 0, 1, u, v)
    return mesh + struct.pack('<I3HBBI', 6, 0, 1, 2, 0, 0, 0)


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class PuppetStaticMesh(unittest.TestCase):
    def test_image_draws_unskinned_triangle_with_authored_uvs(self):
        with tempfile.TemporaryDirectory(prefix='lwe-puppet-static-') as directory:
            root = Path(directory)
            for folder in ('models', 'materials', 'shaders'):
                (root / folder).mkdir()

            (root / 'models/probe.mdl').write_bytes(triangle('materials/probe.json', [(0.25, 0.75)] * 3))
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

    def test_animated_mesh_samples_extracted_frame_and_quad_samples_atlas(self):
        with tempfile.TemporaryDirectory(prefix='lwe-puppet-frame-') as directory:
            root = Path(directory)
            for folder in ('models', 'materials'):
                (root / folder).mkdir()

            red, green, blue = (255, 0, 0, 255), (0, 255, 0, 255), (0, 0, 255, 255)
            atlas = (bytes(red) * 64 + bytes(blue) * 64) * 64
            mip = bytes(green) * (64 * 32)
            texture = b'TEXV0005\0TEXI0001\0' + struct.pack('<7I', 0, 4, 128, 64, 128, 64, 0)
            texture += b'TEXB0001\0' + struct.pack('<2I', 1, 2)
            texture += struct.pack('<3I', 128, 64, len(atlas)) + atlas
            texture += struct.pack('<3I', 64, 32, len(mip)) + mip
            texture += b'TEXS0003\0' + struct.pack('<3I', 2, 64, 64)
            texture += struct.pack('<I7f', 0, .25, 0, 0, 64, 0, 0, 64)
            texture += struct.pack('<I7f', 0, 10, 64, 0, 64, 0, 0, 64)
            (root / 'materials/sprite.tex').write_bytes(texture)
            material = 'materials/sprite.json'
            (root / material).write_text(json.dumps({'passes': [{
                'shader': 'genericimage4', 'textures': ['sprite'], 'combos': {'SPRITESHEET': 1},
                'blending': 'translucent', 'cullmode': 'nocull',
                'depthtest': 'disabled', 'depthwrite': 'disabled'}]}))
            (root / 'models/sprite.mdl').write_bytes(triangle(material, [(0, 0), (1, 0), (0, 1)]))
            (root / 'models/mesh.json').write_text(json.dumps({'material': material, 'puppet': 'models/sprite.mdl'}))
            (root / 'models/quad.json').write_text(json.dumps({'material': material}))
            scene = {
                'camera': {'eye': '160 90 100', 'center': '160 90 0', 'up': '0 1 0'},
                'general': {'orthogonalprojection': {'width': 320, 'height': 180},
                            'clearcolor': '0 0 0', 'bloom': False, 'camerafade': False},
                'objects': [
                    {'id': 1, 'name': 'Mesh', 'image': 'models/mesh.json',
                     'origin': '80 90 0', 'size': '64 64', 'scale': '0.25 0.25 1'},
                    {'id': 2, 'name': 'Quad', 'image': 'models/quad.json',
                     'origin': '240 90 0', 'size': '64 64', 'scale': '0.25 0.25 1'},
                ],
            }
            for frames, frame_color in ((1, red), (8, blue)):
                with self.subTest(frames=frames):
                    (root / 'frame.png').unlink(missing_ok=True)
                    frame, output = render_scene(self, root, scene, frames=frames)
                    self.assertEqual(frame.getpixel((240, 90)), green[:3], output)
                    self.assertEqual(frame.getpixel((77, 93)), frame_color[:3], output)


if __name__ == '__main__':
    unittest.main()
