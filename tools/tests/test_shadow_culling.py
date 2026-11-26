"""Native's ordinary shadow raster state draws both triangle windings."""
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest

from scene_render import render_scene
from test_foliage_shadow import prepare_foliage


def prepare_shadow_culling(root, assets, cullmode, reverse_winding=False):
    root, assets = Path(root), Path(assets)
    scene = prepare_foliage(root, assets, 'normal')
    # Keep native raster-index bit 0 unchanged: this tests the ordinary shadow
    # state, not the separate internal flipped-winding state.
    scene['objects'][2].update(scale='1 1 1', angles='0 0 0')
    (root / 'scene.json').write_text(json.dumps(scene))
    material_path = root / 'materials/caster.json'
    material = json.loads(material_path.read_text())
    material['passes'][0]['cullmode'] = cullmode
    material_path.write_text(json.dumps(material))
    if reverse_winding:
        model_path = root / 'models/caster.mdl'
        model = model_path.read_bytes()
        indices = struct.pack('<6H', 0, 1, 2, 0, 2, 3)
        assert model.endswith(indices), 'Foliage fixture index layout changed'
        model_path.write_bytes(model[:-len(indices)] + struct.pack('<6H', 0, 2, 1, 0, 3, 2))
    return scene


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class ShadowCulling(unittest.TestCase):
    def test_default_shadow_state_keeps_both_triangle_windings(self):
        assets = Path(os.environ.get('LWE_TEST_ASSETS', str(Path.home() /
            '.local/share/Steam/steamapps/common/wallpaper_engine/assets')))
        if not (assets / 'shaders/genericimage4.frag').exists():
            self.skipTest('Set LWE_TEST_ASSETS to installed Wallpaper Engine assets')
        white, black = (255, 255, 255), (0, 0, 0)
        for cullmode in ('nocull', 'normal'):
            for reverse_winding in (False, True):
                with self.subTest(cullmode=cullmode, reverse_winding=reverse_winding), \
                        tempfile.TemporaryDirectory(prefix='lwe-shadow-culling-') as directory:
                    root = Path(directory)
                    scene = prepare_shadow_culling(root, assets, cullmode, reverse_winding)
                    image, output = render_scene(self, root, scene, frames=40, fps=30,
                        extra_args=('--msaa', 'off', '--post-processing', 'disabled',
                                    '--assets-dir', str(assets)))
                    # Stock foliage moves the caster +300 in X/Y. Both point and
                    # spot probes must see clear old space and two shadowed halves.
                    # Native state 4 uses no culling even with cullmode=normal;
                    # explicit nocull cases control the geometry and deformation.
                    self.assertEqual([image.getpixel((round((i + .5) * 320 / 6), 90))
                                      for i in range(6)], [white, black, black] * 2, output)
                    self.assertNotIn('Failed to setup', output)


if __name__ == '__main__':
    unittest.main()
