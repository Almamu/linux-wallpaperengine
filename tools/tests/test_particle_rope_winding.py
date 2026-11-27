"""Rope back-face culling across camera projection and layer transforms."""
import json
import os
from pathlib import Path
import tempfile
import unittest

from scene_render import render_scene


def write_rope_probe(root, renderer, case, culling):
    for directory in ('particles', 'materials'):
        (root / directory).mkdir(parents=True, exist_ok=True)
    perspective = case.startswith('perspective')
    distance, size = (.2, .05) if perspective else (60, 12)
    emitters = [{'name': 'sphererandom', 'rate': 0, 'instantaneous': 1, 'delay': .25,
                 'origin': f'{position} 0 0', 'distancemin': 0, 'distancemax': 0}
                for position in (-distance, distance)]
    definition = {'flags': 5 if perspective else 1, 'maxcount': 2,
                  'material': 'materials/probe.json', 'emitter': emitters,
                  'initializer': [{'name': 'lifetimerandom', 'min': 100, 'max': 100},
                                  {'name': 'sizerandom', 'min': size, 'max': size}],
                  'operator': [{'name': 'angularmovement', 'force': '0 0 0', 'drag': 0}],
                  'renderer': [{'name': renderer, 'subdivision': 1, 'segments': 12, 'length': 60}]}
    layer = {'id': 1, 'name': 'Rope winding probe', 'origin': '160 90 0',
             'particle': 'particles/probe.json', 'instanceoverride': {'size': 1, 'rate': 1, 'count': 1, 'speed': 1, 'lifetime': 1}}
    scene = {'version': 1, 'objects': [layer],
             'camera': {'eye': '160 90 100', 'center': '160 90 0', 'up': '0 1 0'},
             'general': {'orthogonalprojection': {'width': 320, 'height': 180},
                         'clearcolor': '0 0 0', 'bloom': False, 'camerapreview': True,
                         'camerafade': False, 'hdr': False}}
    if perspective:
        scene['camera'] = {'eye': '0 0 1', 'center': '0 0 0', 'up': '0 1 0'}
        scene['general'].update(orthogonalprojection=None, fov=50, nearz=.1, farz=1000)
        layer['origin'] = '0 0 0'
    if case.endswith('mirror'):
        layer['scale'] = '-1 1 1'
    if 'local' in case:
        definition['flags'] &= ~1
    if case.endswith('tilted'):
        layer['angles'] = '0 .4 .6'
    if renderer == 'ropetrail':
        definition['maxcount'] = 1
        definition['emitter'] = emitters[:1]
        speed = .4 if perspective else 120
        definition['initializer'].append({'name': 'velocityrandom', 'min': f'{speed} 0 0', 'max': f'{speed} 0 0'})
        definition['operator'] = [{'name': 'movement', 'drag': 0, 'gravity': '0 0 0'}]
    # Ignore startup/shader compilation delta and hold after a fixed amount of simulation.
    layer['origin'] = {'value': layer['origin'], 'script': """let ticks=0;
export function update(value) {
    ticks++;
    thisLayer.instance.rate=ticks<=80 ? .01/engine.frametime : 0;
    return value;
}"""}
    material = {'passes': [{'shader': 'genericparticle', 'textures': ['util/white'],
                            'blending': 'normal', 'depthtest': 'disabled',
                            'depthwrite': 'disabled', 'cullmode': culling}]}
    for filename, value in [('particles/probe.json', definition), ('materials/probe.json', material),
                            ('scene.json', scene), ('project.json', {'type': 'scene', 'file': 'scene.json', 'title': 'Rope winding probe'})]:
        (root / filename).write_text(json.dumps(value))
    return scene


def visible_pixels(frame):
    from PIL import ImageChops

    red, green, blue = frame.split()
    brightest = ImageChops.lighter(ImageChops.lighter(red, green), blue)
    return sum(brightest.histogram()[21:])


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics integration tests')
class ParticleRopeWinding(unittest.TestCase):
    def render_culling_pair(self, renderer, case):
        counts = []
        for culling in ('nocull', 'normal'):
            with tempfile.TemporaryDirectory(prefix='lwe-rope-') as directory:
                root = Path(directory)
                scene = write_rope_probe(root, renderer, case, culling)
                frame, _ = render_scene(self, root, scene, frames=100, fps=60)
                counts.append(visible_pixels(frame))
        self.assertGreater(counts[0], 20, 'Unculled rope must be visible')
        return counts

    def test_rope_remains_visible_with_native_backface_culling(self):
        for renderer in ('rope', 'ropetrail'):
            for case in ('ortho', 'perspective', 'perspective-mirror', 'perspective-local', 'perspective-tilted'):
                with self.subTest(renderer=renderer, case=case):
                    unculled, culled = self.render_culling_pair(renderer, case)
                    self.assertGreater(culled, unculled * .9, (unculled, culled))

    def test_local_reflection_preserves_native_back_facing_body(self):
        for renderer in ('rope', 'ropetrail'):
            for case in ('ortho-local-mirror', 'perspective-local-mirror'):
                with self.subTest(renderer=renderer, case=case):
                    unculled, culled = self.render_culling_pair(renderer, case)
                    if renderer == 'rope':
                        self.assertEqual(culled, 0)
                    else:
                        # Native may expose a small reversed history end segment;
                        # the reflected trail body must remain back-facing.
                        self.assertLess(culled, unculled * .1, (unculled, culled))


if __name__ == '__main__':
    unittest.main()
