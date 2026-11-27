"""Render eventspawn birth placement, detachment, and pool capacity."""
import json
import os
from pathlib import Path
import tempfile
import unittest

from scene_render import render_scene


STEP_SCRIPT = '''let advanced = 0;
export function init(value) {
    thisLayer.instance.rate = 0;
    return value;
}
export function update(value) {
    thisLayer.instance.rate = 0;
    if (advanced < STEPS && engine.frametime > .0001 && engine.frametime < .08) {
        thisLayer.instance.rate = .05 / engine.frametime;
        ++advanced;
    }
    return value;
}'''


def write_birth_probe(root, *, delay=0):
    for folder in ('particles', 'materials'):
        (root / folder).mkdir(parents=True)
    (root / 'materials/white.json').write_text(json.dumps({'passes': [{
        'shader': 'genericparticle', 'textures': ['util/white'], 'blending': 'additive',
        'cullmode': 'nocull', 'depthtest': 'disabled', 'depthwrite': 'disabled',
    }]}))
    template = {
        'maxcount': 1, 'material': 'materials/white.json', 'operator': [],
        'renderer': [{'name': 'sprite'}],
        'initializer': [{'name': 'lifetimerandom', 'min': 2, 'max': 2},
                        {'name': 'sizerandom', 'min': 12, 'max': 12}],
    }
    emitter = {'name': 'sphererandom', 'rate': 0, 'distancemin': 0, 'distancemax': 0,
               'speedmin': 0, 'speedmax': 0}
    child = dict(template,
        emitter=[dict(emitter, instantaneous=1, origin='20 0 0', delay=delay)],
        initializer=[*template['initializer'], {'name': 'colorrandom', 'min': '0 255 0', 'max': '0 255 0'}])
    (root / 'particles/child.json').write_text(json.dumps(child))
    parent_lifetime = .2 if delay else 2
    steps = 12 if delay else 1
    parent_initializers = [dict(template['initializer'][0], min=parent_lifetime, max=parent_lifetime),
                           template['initializer'][1],
                           {'name': 'colorrandom', 'min': '255 0 0', 'max': '255 0 0'}]
    if delay:
        parent_initializers.append({'name': 'velocityrandom', 'min': '160 0 0', 'max': '160 0 0'})
    objects = []
    for index, (label, instantaneous, x, event) in enumerate((
            ('birth', 1, 48, 'eventspawn'), ('no-birth', 0, 144, 'eventspawn'),
            ('follow-control', 1, 240, 'eventfollow'))):
        parent = dict(template,
            emitter=[dict(emitter, instantaneous=instantaneous, origin='16 0 0')],
            initializer=parent_initializers,
            operator=[{'name': 'movement', 'drag': 0, 'gravity': '0 0 0'}] if delay else [],
            children=[{'name': 'particles/child.json', 'type': event}])
        (root / f'particles/{label}.json').write_text(json.dumps(parent))
        objects.append({'id': index + 1, 'name': label, 'particle': f'particles/{label}.json',
                        'origin': f'{x} 90 0',
                        'visible': {'value': True, 'script': STEP_SCRIPT.replace('STEPS', str(steps))}})
    return {'camera': {'eye': '160 90 100', 'center': '160 90 0', 'up': '0 1 0'},
            'general': {'orthogonalprojection': {'width': 320, 'height': 180},
                        'clearcolor': '0 0 0', 'bloom': False}, 'objects': objects}



def write_capacity_probe(root):
    scene = write_birth_probe(root)
    source = json.loads((root / 'particles/birth.json').read_text())
    source['maxcount'] = 2
    source['emitter'].append(dict(source['emitter'][0], origin='56 0 0'))
    objects = []
    for capacity, origin in ((1, 64), (2, 224)):
        particle = f'particles/capacity-{capacity}.json'
        definition = dict(source, children=[dict(source['children'][0], maxcount=capacity)])
        (root / particle).write_text(json.dumps(definition))
        objects.append(dict(scene['objects'][0], id=capacity, particle=particle, origin=f'{origin} 90 0'))
    scene['objects'] = objects
    return scene



def write_recycle_probe(root, steps):
    scene = write_birth_probe(root)
    child_path = root / 'particles/child.json'
    child = json.loads(child_path.read_text())
    child['initializer'][0].update(min=.6, max=.6)
    child['initializer'][-1].update(min='0 64 0', max='0 64 0')
    child_path.write_text(json.dumps(child))
    parent = json.loads((root / 'particles/birth.json').read_text())
    parent['initializer'][0].update(min=.1, max=.1)
    parent['emitter'][0].update(instantaneous=0, rate=0)
    script = STEP_SCRIPT.replace('STEPS', str(steps)).replace('++advanced;',
        'if (advanced === 0 || advanced === 6 || advanced === 18) thisLayer.emitParticles(1); ++advanced;')
    objects = []
    for capacity, origin in ((1, 64), (2, 224)):
        particle = f'particles/periodic-{capacity}.json'
        definition = dict(parent, children=[dict(parent['children'][0], maxcount=capacity)])
        (root / particle).write_text(json.dumps(definition))
        objects.append(dict(scene['objects'][0], id=capacity, particle=particle, origin=f'{origin} 90 0',
                            visible={'value': True, 'script': script}))
    scene['objects'] = objects
    return scene



def write_transform_probe(root):
    scene = write_birth_probe(root)
    parent_path = root / 'particles/birth.json'
    parent = json.loads(parent_path.read_text())
    parent['children'][0].update(origin='30 0 0', scale='2 1 1', angles='0 0 1.5707963267948966')
    parent_path.write_text(json.dumps(parent))
    child_path = root / 'particles/child.json'
    child = json.loads(child_path.read_text())
    child['emitter'][0]['origin'] = '10 0 0'
    child_path.write_text(json.dumps(child))
    scene['objects'] = [dict(scene['objects'][0], scale='2 2 1')]
    return scene


def count_color(frame, left, right, channel):
    crop = frame.crop((left, 65, right, 115))
    return sum(pixel[channel] >= 160 and all(pixel[i] <= 40 for i in range(3) if i != channel)
               for pixel in (crop.get_flattened_data() if hasattr(crop, 'get_flattened_data') else crop.getdata()))


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class ParticleEventSpawn(unittest.TestCase):
    def test_instantaneous_child_appears_at_parent_birth(self):
        with tempfile.TemporaryDirectory(prefix='lwe-eventspawn-birth-') as directory:
            root = Path(directory)
            frame, _ = render_scene(self, root, write_birth_probe(root), frames=10, fps=30)
            self.assertGreater(count_color(frame, 56, 72, 0), 0, 'Parent birth control never emitted')
            self.assertGreater(count_color(frame, 268, 285, 1), 0, 'Plain eventfollow control never emitted')
            self.assertEqual(count_color(frame, 143, 215, 1), 0, 'Child appeared without a parent birth')
            self.assertGreater(count_color(frame, 76, 93, 1), 0, 'eventspawn child missing at parent birth point')

    def test_delayed_child_survives_parent_and_emits_at_birth_anchor(self):
        with tempfile.TemporaryDirectory(prefix='lwe-eventspawn-anchor-') as directory:
            root = Path(directory)
            frame, _ = render_scene(self, root, write_birth_probe(root, delay=.35), frames=32, fps=30)
            self.assertEqual(count_color(frame, 40, 320, 0), 0, 'Parents must have expired before sampling')
            self.assertEqual(count_color(frame, 143, 215, 1), 0, 'Child appeared without a parent birth')
            self.assertEqual(count_color(frame, 235, 320, 1), 0, 'eventfollow child must stop pending emission at death')
            self.assertGreater(count_color(frame, 76, 93, 1), 0, 'Detached child must emit at the birth anchor')
            self.assertEqual(count_color(frame, 100, 140, 1), 0, 'Detached child moved with its parent')


    def test_child_cap_applies_when_two_parent_particles_share_a_birth_step(self):
        with tempfile.TemporaryDirectory(prefix='lwe-eventspawn-cap-') as directory:
            root = Path(directory)
            frame, _ = render_scene(self, root, write_capacity_probe(root), frames=10, fps=30)
            for x in (80, 120, 240, 280):
                self.assertGreater(count_color(frame, x - 7, x + 8, 0), 0, 'Both parent births must be present')
            for cap, positions in ((1, (100, 140)), (2, (260, 300))):
                visible = sum(count_color(frame, x - 7, x + 8, 1) > 0 for x in positions)
                self.assertEqual(visible, cap, 'The authored child cap limits concurrent eventspawn instances')


    def test_detached_slots_overlap_and_recycle_across_parent_generations(self):
        for steps in (10, 22):
            with self.subTest(steps=steps), tempfile.TemporaryDirectory(prefix='lwe-eventspawn-reuse-') as directory:
                root = Path(directory)
                frame, _ = render_scene(self, root, write_recycle_probe(root, steps), frames=steps+12, fps=30)
                one, two = (frame.crop((x-3, 87, x+4, 94)).getextrema()[1][1] for x in (100, 260))
                self.assertGreater(one, 10, 'The capacity-one slot must emit after the first generation expires')
                self.assertGreater(two, 10, 'The capacity-two pool must still emit after recycling')
                if steps == 10:
                    self.assertAlmostEqual(two, 2*one, delta=4,
                        msg='Two detached child systems can overlap even with only one parent slot')


    def test_child_descriptor_transform_composes_after_birth_translation(self):
        with tempfile.TemporaryDirectory(prefix='lwe-eventspawn-transform-') as directory:
            root = Path(directory)
            frame, _ = render_scene(self, root, write_transform_probe(root), frames=10, fps=30)
            # Parent anchor (80,90), scaled authored child origin (+60,0),
            # and child-local (+10,0) rotated and scaled to (0,-40) in image pixels.
            self.assertGreater(count_color(frame, 73, 88, 0), 0, 'Scaled parent anchor must be visible')
            self.assertGreater(frame.crop((135, 45, 146, 56)).getextrema()[1][1], 160,
                               'Child translation, rotation, and scale must compose at birth')
            self.assertEqual(frame.crop((73, 45, 88, 56)).getextrema()[1][1], 0,
                             'Child descriptor translation was dropped')


if __name__ == '__main__':
    unittest.main()
