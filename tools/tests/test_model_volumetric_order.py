"""Mixed model passes stay together before subsequent volumetric light draws.

Deep Sea Dive's native gate draw precedes the point-volume composite. RenderDoc
pixel history shows identical native/fork gate shader output, but the fork's
late translucent pass erased the already-composited volume contribution.
"""
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest

from scene_render import render_scene


def write_model(root, name, surfaces):
    data = bytearray(b'MDLV0023\0')
    data += struct.pack('<III', 15, 1, len(surfaces))
    for material, z, size in surfaces:
        data += f'materials/{material}.json\0'.encode()
        data += struct.pack('<I6fII', 0, -size, -size, z, size, size, z, 15, 4 * 48)
        for x, y in ((-size, -size), (size, -size), (size, size), (-size, size)):
            data += struct.pack('<12f', x, y, z, 0, 0, 1, 1, 0, 0, 1, 0, 0)
        data += struct.pack('<I6HBBI', 12, 0, 1, 2, 0, 2, 3, 0, 0, 0)
    (root / f'models/{name}.mdl').write_bytes(data)


def prepare(root, gate_blending='translucent', volume=True):
    for directory in ('models', 'materials'):
        (root / directory).mkdir(parents=True)
    write_model(root, 'mixed', [('wall', -7, 10), ('gate', -6, 10)])
    write_model(root, 'sentinel', [('sentinel', 0, .3)])
    for name, blending, color in (('wall', 'normal', '0 0 0'),
                                  ('gate', gate_blending, '0 0 0'),
                                  ('sentinel', 'normal', '0 1 0')):
        (root / f'materials/{name}.json').write_text(json.dumps({'passes': [{
            'shader': 'generic4', 'textures': ['util/white'],
            'constantshadervalues': {'color': color, 'alpha': 1},
            'combos': {'LIGHTING': 0, 'FOG': 0}, 'blending': blending,
            'depthtest': 'enabled', 'depthwrite': 'enabled', 'cullmode': 'nocull'}]}))
    return {
        'version': 5,
        'camera': {'eye': '0 0 12', 'center': '0 0 0', 'up': '0 1 0'},
        'general': {'orthogonalprojection': None, 'nearz': .1, 'farz': 100,
                    'fov': 45, 'ambientcolor': '0 0 0', 'skylightcolor': '0 0 0',
                    'clearcolor': '0 0 0', 'clearenabled': True, 'bloom': False,
                    'hdr': False, 'camerafade': False, 'transparentsorting': True,
                    'lightconfig': {'point': 1, 'spot': 0, 'pointshadow': 0,
                                    'spotshadow': 0, 'directional': 0}},
        'objects': [
            {'id': 1, 'name': 'Opaque wall and translucent gate', 'model': 'models/mixed.mdl'},
            {'id': 2, 'name': 'Point volume', 'light': 'lpoint', 'origin': '0 0 0',
             'radius': 4, 'color': '1 .5 .25', 'intensity': 8, 'density': 1,
             'volumetricsexponent': 1, 'castvolumetrics': volume, 'castshadow': False},
            # This later opaque model exposes splitting the first model across
            # the light's flush boundary; it does not cover the measured center.
            {'id': 4, 'name': 'Opaque sort sentinel', 'model': 'models/sentinel.mdl',
             'origin': '5 3 0'},
            {'id': 3, 'name': 'Camera', 'camera': 'default', 'origin': '0 0 12',
             'angles': '0 0 0', 'fov': 45}],
    }


def prepare_foreground(root, after_light, volume=True, intervening_image=False):
    scene = prepare(root, gate_blending='normal', volume=volume)
    write_model(root, 'foreground', [('foreground', 0, 1.5)])
    (root / 'materials/foreground.json').write_bytes((root / 'materials/wall.json').read_bytes())
    foreground = {'id': 5, 'name': 'Opaque foreground', 'model': 'models/foreground.mdl'}
    scene['objects'].insert(2 if after_light else 1, foreground)
    if intervening_image:
        # A normal image is an admitted volume barrier too. Native draws it in
        # the first phase; merely skipping opaque model barriers is insufficient.
        (root / 'shaders').mkdir()
        (root / 'shaders/black.vert').write_text('''attribute vec2 a_TexCoord;
void main(){gl_Position=vec4(a_TexCoord*2.0-1.0,0.0,1.0);}
''')
        (root / 'shaders/black.frag').write_text('void main(){gl_FragColor=vec4(0,0,0,1);}\n')
        (root / 'materials/black.json').write_text(json.dumps({'passes': [{
            'shader': 'black', 'textures': ['util/white'], 'blending': 'normal',
            'depthtest': 'disabled', 'depthwrite': 'disabled', 'cullmode': 'nocull'}]}))
        (root / 'models/black.json').write_text(json.dumps({
            'material': 'materials/black.json', 'width': 32, 'height': 32}))
        scene['objects'].insert(2, {'id': 6, 'name': 'Intervening normal image',
            'image': 'models/black.json', 'origin': '0 0 0', 'size': '32 32'})
    return scene


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class ModelVolumetricOrder(unittest.TestCase):
    def test_mixed_model_gate_does_not_erase_later_point_volume(self):
        centers = {}
        with tempfile.TemporaryDirectory(prefix='lwe-model-volume-order-') as directory:
            for name, blending, volume in (('opaque-control', 'normal', True),
                                           ('disabled-control', 'translucent', False),
                                           ('mixed', 'translucent', True)):
                root = Path(directory) / name
                frame, output = render_scene(self, root, prepare(root, blending, volume),
                    frames=15, fps=30, extra_args=('--msaa', 'off', '--post-processing', 'disabled'))
                self.assertNotIn('Failed to setup', output)
                self.assertGreater(frame.getpixel((250, 36))[1], 200,
                                   'The later opaque model must actually render')
                centers[name] = frame.getpixel((160, 90))
        self.assertGreater(centers['opaque-control'][0], 80, centers)
        self.assertEqual(centers['disabled-control'], (0, 0, 0), centers)
        for actual, expected in zip(centers['mixed'], centers['opaque-control']):
            self.assertAlmostEqual(actual, expected, delta=3,
                msg=f'The mixed model must finish before the volume composite: {centers}')

    def test_opaque_foreground_before_or_after_light_retains_volume(self):
        centers = {}
        with tempfile.TemporaryDirectory(prefix='lwe-model-volume-foreground-') as directory:
            for name, after_light, volume, image in (('before-light', False, True, False),
                                                    ('disabled', True, False, False),
                                                    ('after-light', True, True, False),
                                                    ('after-image', True, True, True)):
                root = Path(directory) / name
                scene = prepare_foreground(root, after_light, volume, image)
                frame, output = render_scene(self, root, scene,
                    frames=15, fps=30, extra_args=('--msaa', 'off', '--post-processing', 'disabled'))
                self.assertNotIn('Failed to setup', output)
                self.assertGreater(frame.getpixel((250, 36))[1], 200)
                centers[name] = frame.getpixel((160, 90))
        self.assertGreater(centers['before-light'][0], 20, centers)
        self.assertEqual(centers['disabled'], (0, 0, 0), centers)
        for name in ('after-light', 'after-image'):
            for actual, expected in zip(centers[name], centers['before-light']):
                self.assertAlmostEqual(actual, expected, delta=3,
                    msg=f'An opaque foreground must not erase the pending volume: {centers}')

    def test_disabled_sorting_and_custom_order_keep_the_authored_volume_barrier(self):
        for sorting, custom in ((False, False), (True, True)):
            with self.subTest(sorting=sorting, custom=custom), tempfile.TemporaryDirectory(
                    prefix='lwe-model-volume-authored-') as directory:
                root = Path(directory)
                scene = prepare_foreground(root, after_light=True, intervening_image=True)
                scene['general'].update(transparentsorting=sorting, customsortorder=custom)
                frame, output = render_scene(self, root, scene,
                    frames=15, fps=30, extra_args=('--msaa', 'off', '--post-processing', 'disabled'))
                self.assertNotIn('Failed to setup', output)
                self.assertGreater(frame.getpixel((250, 36))[1], 200)
                self.assertEqual(frame.getpixel((160, 90)), (0, 0, 0))


if __name__ == '__main__':
    unittest.main()
