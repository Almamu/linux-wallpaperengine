"""Native spot directions retain authored and inherited scale in shader uniforms."""
import json
import os
from pathlib import Path
import tempfile
import unittest

from scene_render import render_scene
from shadow_depth import prepare
from volume_scene import prepare_volume_scene


def prepare_surface_probe(root, assets):
    scene = prepare(root, assets, 9990)
    spot = scene['objects'][1]
    spot.update(parent=5, scale='4000 4000 4000', angles='0 0 -1.74533')
    scene['objects'].append({'id': 5, 'name': 'Scaled light parent', 'solid': True,
                             'scale': '.00031 .00031 .00031'})
    path = root / 'shaders/genericimage4.frag'
    stock = path.read_text()
    path.write_text(stock[:stock.index('void main()')] + '''
void main() {
    int cell = int(floor(v_TexCoord.x * 4.0));
    vec3 direction = g_LSpot_Direction[0].xyz;
    float value = length(direction) * 0.5;
    if (cell == 0) value = direction.x * 0.25 + 0.5;
    if (cell == 1) value = direction.y * 0.25 + 0.5;
    if (cell == 2) value = direction.z * 0.25 + 0.5;
    gl_FragColor = vec4(CAST3(value), 1.0);
}
''')
    (root / 'scene.json').write_text(json.dumps(scene))
    return scene


def prepare_volume_probe(root, assets):
    scene = prepare_volume_scene(root)
    scene['objects'][0].update(parent=5, origin='0 0 0', scale='4000 4000 4000')
    scene['objects'].append({'id': 5, 'name': 'Scaled light parent', 'solid': True,
                             'origin': '-4 0 0', 'scale': '.00031 .00031 .00031'})
    stock = (Path(assets) / 'shaders/volumetricsfront.frag').read_text()
    (root / 'shaders').mkdir(exist_ok=True)
    (root / 'shaders/volumetricsfront.frag').write_text(stock[:stock.index('void main()')] + '''
void main() {
    gl_FragColor = vec4(CAST3(length(g_RenderVar3.xyz) * 0.5), 1.0);
}
''')
    (root / 'scene.json').write_text(json.dumps(scene))
    return scene


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class ScaledSpotDirection(unittest.TestCase):
    def assets(self):
        return Path(os.environ.get('LWE_TEST_ASSETS',
            Path.home() / '.local/share/Steam/steamapps/common/wallpaper_engine/assets'))

    def test_surface_uniform_retains_parent_and_light_scale(self):
        with tempfile.TemporaryDirectory(prefix='lwe-scaled-spot-surface-') as directory:
            root = Path(directory)
            scene = prepare_surface_probe(root, self.assets())
            image, output = render_scene(self, root, scene, frames=20)
            self.assertNotIn('GLSL fragment unit parsing Failed', output)
            # Native copies world column zero: (-.21532465, -1.22116145, 0), norm 1.24.
            for x, expected in zip((40, 120, 200, 280), (114, 50, 128, 158)):
                with self.subTest(x=x):
                    self.assertLessEqual(max(abs(channel - expected)
                                             for channel in image.getpixel((x, 90))), 1)

    def test_volume_uniform_retains_parent_and_light_scale(self):
        with tempfile.TemporaryDirectory(prefix='lwe-scaled-spot-volume-') as directory:
            root = Path(directory)
            scene = prepare_volume_probe(root, self.assets())
            image, output = render_scene(self, root, scene)
            self.assertNotIn('GLSL fragment unit parsing Failed', output)
            # Native RenderVar3 receives the same raw world column; geometry stays normalized.
            self.assertLessEqual(max(abs(channel - 158)
                                     for channel in image.getpixel((160, 90))), 1)
