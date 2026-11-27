"""Timed particle emission must expire even when no particle slots are available."""
import copy
import json
import os
from pathlib import Path
import tempfile
import unittest

from scene_render import render_scene


def write_duration_probe(root, kind, case, candidate=False):
    for directory in ('particles', 'materials', 'shaders'):
        (root / directory).mkdir(parents=True, exist_ok=True)
    layer = {'id': 1, 'name': 'Timed emitter', 'origin': '80 90 0',
             'particle': 'particles/probe.json', 'instanceoverride': {}}
    scene = {'camera': {'eye': '160 90 100', 'center': '160 90 0', 'up': '0 1 0'},
             'general': {'orthogonalprojection': {'width': 320, 'height': 180},
                         'clearcolor': '0 0 0', 'bloom': False}, 'objects': [layer]}
    definition = {'flags': 1, 'maxcount': 1, 'material': 'materials/probe.json',
                  'emitter': [{'name': kind, 'rate': 100, 'duration': .5,
                               'distancemin': 0, 'distancemax': 0}],
                  'initializer': [{'name': 'lifetimerandom', 'min': 1, 'max': 1},
                                  {'name': 'sizerandom', 'min': 60, 'max': 60},
                                  {'name': 'velocityrandom', 'controlpoint': 1,
                                   'min': '40 -20 10', 'max': '40 -20 10'}],
                  'operator': [{'name': 'angularmovement', 'drag': 0, 'force': '0 0 0'}],
                  'renderer': [{'name': 'spritetrail'}]}
    if case == 'roomy':
        definition['maxcount'] = 128
        definition['initializer'][0].update(min=.2, max=.2)
    elif case == 'delayed-full':
        definition['emitter'][0]['delay'] = .1
        definition['emitter'].insert(0, {'name': kind, 'rate': 0, 'instantaneous': 1,
                                         'distancemin': 0, 'distancemax': 0})
    elif case in ('delayed', 'paused', 'restart'):
        definition['initializer'][0].update(min=.2, max=.2)
        if case == 'delayed':
            definition['emitter'][0]['delay'] = .8
        elif case == 'paused':
            layer['origin'] = {'value': layer['origin'], 'script': '''let elapsed=0, resumed=false;
export function init(value) {thisLayer.pause();return value;}
export function update(value) {
    elapsed+=engine.frametime;
    if(elapsed>.6 && !resumed) {thisLayer.play();resumed=true;}
    return value;
}'''}
        else:
            layer['origin'] = {'value': layer['origin'], 'script': '''let elapsed=0, restarted=false;
export function update(value) {
    elapsed+=engine.frametime;
    if(elapsed>1.2 && !restarted) {thisLayer.stop();thisLayer.play();restarted=true;}
    return value;
}'''}
    elif case == 'single-tick':
        definition['emitter'][0]['duration'] = .01
        definition['initializer'][0].update(min=100, max=100)
        layer['origin'] = {'value': layer['origin'], 'script': '''let ticks=0;
export function init(value) {thisLayer.instance.rate=0;return value;}
export function update(value) {
    ticks++;
    thisLayer.instance.rate=ticks==10 ? .05/engine.frametime : 0;
    return value;
}'''}
    elif case != 'full':
        raise ValueError(case)
    # A separate, continuously emitting system guards against a blank/broken renderer.
    control = copy.deepcopy(definition)
    control['emitter'] = [{'name': kind, 'rate': 100, 'distancemin': 0, 'distancemax': 0}]
    scene['objects'].append({'id': 2, 'name': 'Continuous control', 'origin': '240 90 0',
                             'particle': 'particles/control.json'})
    # Keep the same diagnostic particle visible in both candidate and reference captures.
    y_sign = '-1.0' if candidate else '1.0'
    (root / 'shaders/genericparticle.vert').write_text('''
attribute vec3 a_Position;
attribute vec4 a_TexCoordVec4;
attribute vec4 a_TexCoordVec4C1;
varying vec4 v_Color;
#if GS_ENABLED
varying vec3 v_Rotation;
varying vec4 v_VelocityLifetime;
#else
uniform mat4 g_ModelViewProjectionMatrix;
varying vec2 v_TexCoord;
#endif
void main() {
    v_Color=vec4(vec3(.5,.5,.5)+a_TexCoordVec4C1.xyz*vec3(1.0,Y_SIGN,1.0)/200.0,1.0);
#if GS_ENABLED
    gl_Position=vec4(a_Position,60.0);
    v_Rotation=vec3(0.0,0.0,0.0);
    // Keep zero velocity particles visible instead of degenerating the trail.
    v_VelocityLifetime=vec4(20.0,0.0,0.0,0.0);
#else
    vec3 position=a_Position+vec3((a_TexCoordVec4.xy-.5)*60.0,0.0);
    gl_Position=mul(vec4(position,1.0),g_ModelViewProjectionMatrix);
    v_TexCoord=a_TexCoordVec4.xy;
#endif
}
'''.replace('Y_SIGN', y_sign))
    (root / 'shaders/genericparticle.frag').write_text('''
varying vec4 v_Color;
void main() {
    gl_FragColor=v_Color;
}
''')
    material = {'passes': [{'shader': 'genericparticle', 'textures': ['util/white'],
                           'blending': 'normal', 'depthtest': 'disabled',
                           'depthwrite': 'disabled', 'cullmode': 'nocull'}]}
    for filename, data in [('particles/probe.json', definition), ('particles/control.json', control),
                           ('materials/probe.json', material), ('scene.json', scene),
                           ('project.json', {'type': 'scene', 'file': 'scene.json',
                                             'title': 'Timed emitter probe'})]:
        (root / filename).write_text(json.dumps(data))
    return scene


def visible_regions(frame):
    return [frame.crop(bounds).getbbox() is not None for bounds in
            ((0, 0, 160, 180), (160, 0, 320, 180))]


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics integration tests')
class ParticleEmissionDuration(unittest.TestCase):
    def check_frame(self, case, seconds, expected):
        for kind in ('boxrandom', 'sphererandom'):
            with self.subTest(kind=kind, case=case, seconds=seconds):
                with tempfile.TemporaryDirectory(prefix='lwe-duration-') as directory:
                    root = Path(directory)
                    scene = write_duration_probe(root, kind, case, candidate=True)
                    frame, output = render_scene(self, root, scene, fps=20, frames=round(seconds * 20))
                    self.assertEqual(visible_regions(frame), [expected, True], output)

    def test_full_pool_initially_emits(self):
        self.check_frame('full', .3, True)

    def test_full_pool_does_not_extend_duration(self):
        self.check_frame('full', 1.8, False)

    def test_roomy_pool_honors_duration(self):
        self.check_frame('roomy', 1.8, False)

    def test_delay_and_duration_advance_while_another_emitter_fills_the_pool(self):
        self.check_frame('delayed-full', 1.8, False)

    def test_duration_starts_after_the_initial_delay(self):
        self.check_frame('delayed', 1.1, True)
        self.check_frame('delayed', 1.8, False)

    def test_script_pause_preserves_remaining_emission_time(self):
        self.check_frame('paused', .3, False)
        self.check_frame('paused', .9, True)
        self.check_frame('paused', 1.8, False)

    def test_final_active_tick_can_emit_before_duration_expires(self):
        self.check_frame('single-tick', 1.0, True)

    def test_stop_then_play_restarts_the_duration(self):
        self.check_frame('restart', 1.4, True)
        self.check_frame('restart', 2.2, False)


if __name__ == '__main__':
    unittest.main()
