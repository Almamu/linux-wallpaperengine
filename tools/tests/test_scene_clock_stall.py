"""SceneScript's accumulated scene time and frame delta agree after a render stall."""
import json
import os
from pathlib import Path
import tempfile
import unittest

from scene_render import render_scene


SCRIPT = '''
let frame = 0;
let previousTime = 0, previousWall = Date.now();
export function update(value) {
    const time = engine.runtime, wall = Date.now();
    if (++frame >= 4 && frame <= 6) {
        console.log('SCENE_CLOCK ' + JSON.stringify({
            dt: engine.frametime, elapsed: time - previousTime, wallMs: wall - previousWall
        }));
    }
    previousTime = time;
    previousWall = wall;
    if (frame === 4) while (Date.now() - wall < 1000) {}
    return value;
}
'''


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class SceneClockStall(unittest.TestCase):
    def test_long_frame_advances_the_scene_once_by_its_capped_delta(self):
        scene = {
            'camera': {'eye': '0 0 1', 'center': '0 0 0', 'up': '0 1 0'},
            'general': {'orthogonalprojection': {'width': 320, 'height': 180},
                        'camerafade': False, 'clearcolor': '0 0 0'},
            'objects': [{'id': 1, 'name': 'clock probe', 'solid': True,
                         'visible': {'value': True, 'script': SCRIPT}}],
        }
        with tempfile.TemporaryDirectory(prefix='lwe-scene-clock-') as directory:
            _, output = render_scene(self, Path(directory), scene, fps=20, frames=8)
        samples = [json.loads(line.split('SCENE_CLOCK ', 1)[1])
                   for line in output.splitlines() if 'SCENE_CLOCK ' in line]
        self.assertEqual(len(samples), 3, output)
        before, stall, after = samples
        for sample in (before, after):
            self.assertGreater(sample['dt'], 0)
            self.assertLess(sample['dt'], .25)
            self.assertAlmostEqual(sample['elapsed'], sample['dt'], places=5)
        self.assertGreaterEqual(stall['wallMs'], 1000)
        self.assertEqual(stall['dt'], .25)
        self.assertAlmostEqual(stall['elapsed'], .25, places=5, msg=output)


if __name__ == '__main__':
    unittest.main()
