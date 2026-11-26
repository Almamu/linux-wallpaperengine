"""Opt-in shadow atlas regression; run on an isolated display under the render lock."""
import json
import os
from pathlib import Path
import tempfile
import unittest

from scene_render import render_scene
from shadow_depth import prepare, sample


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class ShadowDepth(unittest.TestCase):
    def test_native_projection_caster_bias_and_atlas_occupancy(self):
        assets = Path(os.environ.get('LWE_TEST_ASSETS', str(Path.home() /
            '.local/share/Steam/steamapps/common/wallpaper_engine/assets')))
        if not (assets / 'shaders/genericimage4.frag').exists():
            self.skipTest('Set LWE_TEST_ASSETS to installed Wallpaper Engine assets')
        with tempfile.TemporaryDirectory(prefix='lwe-shadow-depth-') as directory:
            for distance in (100, 9000, 9990):
                with self.subTest(distance=distance):
                    root = Path(directory) / f'shadow-{distance}'
                    scene = prepare(root, assets, distance)
                    image, output = render_scene(self, root, scene, frames=40, fps=30,
                        extra_args=('--msaa', 'off', '--post-processing', 'disabled',
                                    '--assets-dir', str(assets), '--noautomute'))
                    result = sample(image)
                    if os.environ.get('LWE_TEST_ARTIFACTS'):
                        (Path(os.environ['LWE_TEST_ARTIFACTS']) / root.name / 'result.json').write_text(
                            json.dumps(result, indent=2) + '\n')
                    self.assertTrue(result['frame_complete'], result)
                    values = result['numeric_values']
                    self.assertAlmostEqual(values['spot_depth_d2'], .0250374094, delta=3e-7)
                    for light in ('point', 'spot'):
                        self.assertGreater(values[light + '_compare_d100'], .99, result)
                        expected = 1.0 if distance == 9990 else 0.0
                        self.assertAlmostEqual(values[light + '_compare_d9990_1'], expected, delta=1e-5)
                    if distance == 9990:
                        self.assertEqual(result['atlas_occupied_pixels'], 0, result)
                    else:
                        self.assertGreater(result['atlas_occupied_pixels'], 20, result)
                    self.assertNotIn('Failed to setup', output)


if __name__ == '__main__':
    unittest.main()
