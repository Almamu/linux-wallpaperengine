"""Native-backed translucent models keep two-sided diffuse lighting opt-in.

Deep Sea Dive's native gate control is pixel-identical with the combo omitted
or explicitly zero. Its DXBC lacks the six diffuse abs operations that the
fork injected. This small back-lit panel exercises that default and explicit 1
through the real generic4 material, light uniforms, and model draw path.
"""
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest

from scene_render import render_scene


def prepare(root, double_sided):
    for directory in ('models', 'materials'):
        (root / directory).mkdir(parents=True)
    vertices = ((-2, -2), (2, -2), (2, 2), (-2, 2))
    data = bytearray(b'MDLV0023\0')
    data += struct.pack('<III', 15, 1, 1) + b'materials/panel.json\0'
    data += struct.pack('<I6fII', 0, -2, -2, 0, 2, 2, 0, 15, 4 * 48)
    for x, y in vertices:
        data += struct.pack('<12f', x, y, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0)
    data += struct.pack('<I6HBBI', 12, 0, 1, 2, 0, 2, 3, 0, 0, 0)
    (root / 'models/panel.mdl').write_bytes(data)
    material_path = root / 'materials/panel.json'
    material = {'passes': [{
        'shader': 'generic4', 'textures': ['util/white'],
        'constantshadervalues': {'color': '.7 .7 .7'}, 'combos': {'LIGHTING': 0, 'FOG': 0},
        'blending': 'normal', 'depthtest': 'enabled', 'depthwrite': 'enabled',
        'cullmode': 'nocull'}]}
    render_pass = material['passes'][0]
    render_pass.update(blending='translucent', depthwrite='disabled')
    render_pass['combos'].update(LIGHTING=1, REFLECTION=0)
    if double_sided is not None:
        render_pass['combos']['DOUBLESIDEDLIGHTING'] = double_sided
    render_pass['constantshadervalues'].update(alpha=.75, metallic=0, roughness=1)
    material_path.write_text(json.dumps(material))
    return {
        'version': 5,
        'camera': {'eye': '0 0 8', 'center': '0 0 0', 'up': '0 1 0'},
        'general': {'orthogonalprojection': None, 'fov': 50,
                    'clearcolor': '0 0 0', 'clearenabled': True,
                    'ambientcolor': '.05 .05 .05', 'skylightcolor': '.05 .05 .05',
                    'bloom': False, 'hdr': False, 'fogdistance': False,
                    'camerafade': False, 'cameraparallax': False},
        'objects': [
            {'id': 1, 'name': 'Back-lit translucent panel', 'model': 'models/panel.mdl'},
            # The panel normal is +Z. Offset the rear light so V+L stays finite.
            {'id': 2, 'light': 'point', 'origin': '3 0 -4', 'radius': 20,
             'intensity': 4, 'color': '1 1 1', 'castshadow': False}],
    }


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class ModelDoubleSidedLighting(unittest.TestCase):
    def test_translucent_default_matches_explicit_zero_and_honors_explicit_one(self):
        from PIL import ImageChops

        frames = {}
        with tempfile.TemporaryDirectory(prefix='lwe-model-double-sided-') as directory:
            for name, combo in (('default', None), ('disabled', 0), ('enabled', 1)):
                root = Path(directory) / name
                frames[name], output = render_scene(self, root, prepare(root, combo),
                    extra_args=('--msaa', 'off', '--post-processing', 'disabled'))
                self.assertNotIn('Failed to setup', output)
        disabled = frames['disabled'].getpixel((160, 90))
        enabled = frames['enabled'].getpixel((160, 90))
        self.assertGreater(min(disabled), 3, 'Ambient light must reveal the panel')
        self.assertGreater(min(enabled), max(disabled) + 20,
                           'Explicit 1 must still light the rear-facing diffuse surface')
        self.assertIsNone(ImageChops.difference(frames['default'], frames['disabled']).getbbox(),
                          'Translucent blending must not implicitly enable two-sided diffuse lighting')


if __name__ == '__main__':
    unittest.main()
