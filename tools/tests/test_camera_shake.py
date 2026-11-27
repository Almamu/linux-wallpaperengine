"""Measure camera shake in a rendered frame against the stock transform."""
import json
import math
import os
from pathlib import Path
import tempfile
import unittest

from scene_render import render_scene


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class CameraShake(unittest.TestCase):
    def test_applies_to_the_orthographic_resting_camera_and_perspective_layer(self):
        from PIL import ImageChops

        for perspective, amplitude, roughness in ((False, 20, 0), (True, 240, .5)):
            with self.subTest(perspective=perspective), tempfile.TemporaryDirectory(prefix='lwe-camera-shake-') as work:
                root = Path(work)
                for folder in ('models', 'materials', 'shaders'):
                    (root / folder).mkdir()
                (root / 'models/time.json').write_text(json.dumps({
                    'fullscreen': True, 'material': 'materials/time.json'}))
                (root / 'materials/time.json').write_text(json.dumps({'passes': [{
                    'shader': 'time', 'blending': 'normal', 'cullmode': 'nocull',
                    'depthtest': 'disabled', 'depthwrite': 'disabled'}]}))
                (root / 'shaders/time.vert').write_text('''
attribute vec2 a_TexCoord;
void main() { gl_Position = vec4(a_TexCoord * 2.0 - 1.0, 0.0, 1.0); }
''')
                # Stamp the scene time in the background of the exact marker frame.
                (root / 'shaders/time.frag').write_text('''
uniform float g_Time;
void main() {
    float ticks = floor(g_Time * 4096.0 + 0.5);
    gl_FragColor = vec4(floor(ticks / 256.0), mod(ticks, 256.0), 0.0, 255.0) / 255.0;
}
''')
                projection = None if perspective else {'width': 320, 'height': 180}
                marker_size = 8 if perspective else 16
                scene = {
                    'camera': {'eye': '0 0 200' if perspective else '160 90 100',
                               'center': '0 0 0' if perspective else '160 90 0', 'up': '0 1 0'},
                    'general': {'orthogonalprojection': projection, 'nearz': .1, 'farz': 1000,
                                'fov': 60, 'zoom': 1, 'bloom': False, 'camerafade': False,
                                'camerashake': True, 'camerashakespeed': 1.1,
                                'camerashakeamplitude': amplitude, 'camerashakeroughness': roughness},
                    'objects': [
                        {'id': 1, 'name': 'Scene time', 'image': 'models/time.json', 'size': '320 180'},
                        {'id': 2, 'name': 'Marker', 'image': 'models/util/solidlayer.json',
                         'origin': '0 0 0' if perspective else '160 90 0',
                         'size': f'{marker_size} {marker_size}', 'color': '1 0 1',
                         'visible': {'value': True, 'script': '''
export function update(value) {
    const pose = thisScene.getCameraTransforms();
    if (pose.eye.x !== 0 || pose.eye.y !== 0 ||
        pose.center.x !== 0 || pose.center.y !== 0 || pose.up.y !== 1)
        throw Error('Camera shake leaked into the SceneScript pose');
    return value;
}
'''}},
                    ]}
                if perspective:
                    scene['objects'].append({'id': 3, 'name': 'Active camera', 'camera': 'default',
                                             'origin': '0 0 100', 'fov': 60, 'zoom': 1})
                frame, output = render_scene(self, root, scene, frames=8)
                red, _, blue = frame.split()
                high, low, _ = frame.getpixel((4, 4))
                phase = ((high * 256 + low) / 4096) * 1.1 ** 2
                self.assertGreater(phase, .2, output)
                dx, dy = math.cos(phase), math.sin(1.333 * phase)
                dz = math.sin(phase) if perspective else 0.
                if roughness:
                    length = math.sqrt(dx * dx + dy * dy + dz * dz)
                    scale = length ** (roughness ** 3 - 1)
                    dx, dy, dz = dx * scale, dy * scale, dz * scale
                displacement_scale = .1 * amplitude * (1 if perspective else .1 * 180)
                dx, dy, dz = dx * displacement_scale, dy * displacement_scale, dz * displacement_scale
                px_per_world_unit = 90 / ((100 + dz) * math.tan(math.radians(30))) if perspective else 1
                self.assertGreater(math.hypot(dx, dy) * px_per_world_unit, 4)
                red_mask = red.point(lambda value: 255 if value > 220 else 0)
                blue_mask = blue.point(lambda value: 255 if value > 220 else 0)
                box = ImageChops.multiply(red_mask, blue_mask).getbbox()
                self.assertIsNotNone(box, output)
                self.assertAlmostEqual((box[0] + box[2]) / 2, 160 - dx * px_per_world_unit, delta=1.25)
                self.assertAlmostEqual((box[1] + box[3]) / 2, 90 + dy * px_per_world_unit, delta=1.25)
                self.assertAlmostEqual(box[2] - box[0], marker_size * px_per_world_unit, delta=1.5)


if __name__ == '__main__':
    unittest.main()
