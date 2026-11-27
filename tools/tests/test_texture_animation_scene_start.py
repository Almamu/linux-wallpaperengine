"""An authored sprite's first rendered frame excludes scene construction time."""
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest

from scene_render import render_scene


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class TextureAnimationSceneStart(unittest.TestCase):
    def test_first_sprite_frame_survives_slow_scene_construction(self):
        with tempfile.TemporaryDirectory(prefix='lwe-sprite-start-') as directory:
            root = Path(directory)
            (root / 'models').mkdir()
            (root / 'materials').mkdir()

            # Red frame 0 lasts 40 ms, green frame 1 lasts ten seconds.
            colors = ((255, 0, 0, 255), (0, 255, 0, 255))
            pixels = b''.join(bytes(color) * 8 for color in colors) * 8
            texture = b'TEXV0005\0TEXI0001\0' + struct.pack('<7I', 0, 7, 16, 8, 8, 8, 0)
            texture += b'TEXB0001\0' + struct.pack('<5I', 1, 1, 16, 8, len(pixels)) + pixels
            texture += b'TEXS0003\0' + struct.pack('<3I', 2, 8, 8)
            texture += struct.pack('<I7f', 0, .04, 0, 0, 8, 0, 0, 8)
            texture += struct.pack('<I7f', 0, 10, 8, 0, 8, 0, 0, 8)
            (root / 'materials/sprite.tex').write_bytes(texture)
            (root / 'models/sprite.json').write_text(json.dumps({'material': 'materials/sprite.json'}))
            (root / 'materials/sprite.json').write_text(json.dumps({'passes': [{
                'shader': 'genericimage2', 'textures': ['sprite'], 'combos': {'spritesheet': 1},
                'blending': 'normal', 'depthtest': 'disabled', 'depthwrite': 'disabled', 'cullmode': 'nocull',
            }]}))
            scene = {
                'camera': {'eye': '160 90 1', 'center': '160 90 0', 'up': '0 1 0'},
                'general': {'orthogonalprojection': {'width': 320, 'height': 180}, 'bloom': False},
                'objects': [{
                    'id': 1, 'name': 'Animated image', 'image': 'models/sprite.json',
                    'origin': '160 90 0', 'size': '64 64',
                    'visible': {'value': True, 'script': '''
const start = Date.now();
while (Date.now() - start < 120) {}
export function update(v) { return v; }
'''},
                }],
            }
            frame, _ = render_scene(self, root, scene, frames=1)
            self.assertEqual(frame.getpixel((160, 90)), colors[0][:3])


if __name__ == '__main__':
    unittest.main()
