"""Authored point and spot volumes keep their shadow slots after a light is removed."""
import os
from pathlib import Path
import tempfile
import unittest

from scene_render import render_scene
from volume_scene import prepare_volume_scene


def prepare_slot_probe(root, kind, shadow, remove):
    scene = prepare_volume_scene(root)
    light = scene['objects'][0]
    if kind == 'point':
        light.update(light='lpoint', origin='0 0 0', radius=4)
    removed = dict(light, id=1, name='Earlier light', castvolumetrics=False)
    scene['objects'].insert(0, removed)
    scene['general']['lightconfig'] = {kind: 2, f'{kind}shadow': int(shadow)}
    light['castshadow'] = shadow
    if remove:
        light['intensity'] = {'value': 8, 'script': '''
let queued = false;
export function update(value) {
    if (!queued) {
        queued = thisScene.destroyLayer(thisScene.getLayer('Earlier light'));
        if (!queued) throw Error('Could not remove earlier light');
    }
    return value;
}
'''}
    # Color marks the selected shader variant while preserving real volume geometry.
    (root / 'shaders/volumetricsfront.frag').write_text('''
void main() {
#if SHADOW
    gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
#else
    gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);
#endif
}
''')
    return scene


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class VolumetricShadowSlots(unittest.TestCase):
    def test_surviving_volumes_keep_their_shadow_variant(self):
        for kind in ('point', 'spot'):
            for shadow, remove in ((False, True), (True, False), (True, True)):
                with self.subTest(kind=kind, shadow=shadow, remove=remove), tempfile.TemporaryDirectory(
                        prefix='lwe-volume-slots-') as directory:
                    root = Path(directory)
                    scene = prepare_slot_probe(root, kind, shadow, remove)
                    frame, output = render_scene(self, root, scene, frames=15, fps=30,
                        extra_args=('--msaa', 'off', '--post-processing', 'disabled'))
                    self.assertNotIn('Failed to setup', output)
                    self.assertEqual(frame.getpixel((160, 90)), (255, 0, 0) if shadow else (0, 255, 0))


if __name__ == '__main__':
    unittest.main()
