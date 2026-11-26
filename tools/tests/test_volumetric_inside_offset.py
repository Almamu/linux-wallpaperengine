"""Native ordinary spot volumes classify the camera 0.2 units along its view."""
import json
import os
from pathlib import Path
import tempfile
import unittest

from scene_render import render_scene
from volume_scene import prepare_volume_scene


def prepare_offset_probe(root, case):
    scene = prepare_volume_scene(root)
    light = scene['objects'][0]
    scene['objects'] = [light]
    if case == 'inside':
        scene['camera'].update(eye='0 0 0', center='1 0 0')
    elif case == 'boundary':
        # Frozen shot-45-2039-t10, light 225. Cone margin is +0.075099
        # at offset 0.1 but -0.016428 at native 140196ce0's offset 0.2.
        scene['camera'].update(eye='-8.191300392150879 4.606060028076172 -6.4450201988220215',
            center='-13.16355037689209 5.095290184020996 -6.249989986419678')
        scene['general']['fov'] = 61.36000061035156
        light.update(origin='-2.134739398956299 10.978424072265625 -5.42417049407959',
                     angles='0 0 -1.74533', innercone=20, outercone=35, radius=35)
    elif case != 'outside':
        raise ValueError(case)
    # Keep real volume geometry and production branch selection. The marker
    # makes FULLSCREEN visible independently of raymarch quality or density.
    (root / 'shaders/volumetricsfront.frag').write_text('''
void main() {
#if FULLSCREEN
    gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
#else
    gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);
#endif
}
''')
    (root / 'scene.json').write_text(json.dumps(scene))
    return scene


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class VolumetricInsideOffset(unittest.TestCase):
    def test_native_camera_offset_selects_the_volume_geometry_branch(self):
        # First prove both marker branches render. The boundary camera then
        # must use cone geometry even though its near-Z plane is only 0.1.
        for case in ('inside', 'outside', 'boundary'):
            with self.subTest(case=case), tempfile.TemporaryDirectory(prefix=f'lwe-volume-offset-{case}-') as directory:
                root = Path(directory)
                frame, output = render_scene(self, root, prepare_offset_probe(root, case),
                    extra_args=('--msaa', 'off', '--post-processing', 'disabled'))
                self.assertNotIn('GLSL fragment unit parsing Failed', output)
                self.assertNotIn('Failed to setup object', output)
                if case in ('inside', 'outside'):
                    expected = (255, 0, 0) if case == 'inside' else (0, 255, 0)
                    self.assertEqual(frame.getpixel((160, 90)), expected)
                else:
                    self.assertEqual(frame.getchannel('R').getextrema(), (0, 0),
                                     'Shot 45 light 225 incorrectly selected FULLSCREEN from near-Z 0.1')


if __name__ == '__main__':
    unittest.main()
