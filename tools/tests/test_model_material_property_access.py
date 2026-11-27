"""Material callbacks read and write uniforms from their own model pass."""
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest

from scene_render import render_scene


def write_fixture(root):
    for folder in ('materials', 'models', 'shaders'):
        (root / folder).mkdir()
    material = b'materials/probe.json\0'
    points = ((-.22, -.25), (.22, -.25), (.22, .25), (-.22, .25))
    vertices = b''.join(struct.pack('<12f', x, y, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0) for x, y in points)
    indices = struct.pack('<6H', 0, 1, 2, 0, 2, 3)
    (root / 'models/probe.mdl').write_bytes(b'MDLV0013\0' + struct.pack('<3I', 15, 1, 1)
        + material + struct.pack('<2I', 0, len(vertices)) + vertices + struct.pack('<I', len(indices)) + indices)
    (root / 'shaders/probe.vert').write_text('''
attribute vec3 a_Position;
uniform float uShift; // {"material":"shift","default":0}
void main() { gl_Position=vec4(a_Position.x+uShift,a_Position.y,0.,1.); }
''')
    (root / 'shaders/probe.frag').write_text('''
uniform vec3 uTint; // {"material":"tint","default":"0 0 1"}
uniform float uGain; // {"material":"gain","default":1}
void main() { gl_FragColor=vec4(uTint*uGain,1.); }
''')
    passes = []
    for side, color, gain in ((-.5, 'props.probe_color', 1), (.5, 'new Vec3(0,1,0)', .5)):
        writer = '''export function applyUserProperties(props) {
    if (props.probe_color === undefined) return;
    const color = COLOR;
    if (thisObject === thisLayer) throw Error('Missing material pass owner');
    thisObject.tint = color.copy();
    const readback = thisObject.tint;
    if (!readback || readback.x !== color.x || readback.y !== color.y || readback.z !== color.z)
        throw Error('Scoped material readback failed');
    if (thisLayer.tint !== undefined) throw Error('Pass uniform leaked onto layer');
}'''.replace('COLOR', color)
        passes.append({'shader':'probe', 'blending':'normal', 'depthtest':'disabled', 'depthwrite':'disabled',
                       'cullmode':'nocull', 'constantshadervalues': {
                           'driver': {'value': 0, 'script': writer}, 'shift': side, 'gain': gain,
                           'tint': {'value': '0 0 1', 'script': 'export function update(value){return value;}'},
                       }})
    (root / 'materials/probe.json').write_text(json.dumps({'passes': passes}))
    scene = {'camera': {'eye': '0 0 3', 'center': '0 0 0', 'up': '0 1 0'},
             'general': {'orthogonalprojection': None, 'nearz': .1, 'farz': 100, 'fov': 45,
                         'clearcolor': '0 0 0', 'clearenabled': True, 'bloom': False},
             'objects': [{'id': 1, 'name': 'Material pass scope', 'model': 'models/probe.mdl'}]}
    project = {'type': 'scene', 'file': 'scene.json', 'title': 'Material pass scope',
               'general': {'properties': {'probe_color': {'type': 'color', 'value': '1 0 0'}}}}
    return scene, project


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class ModelMaterialPropertyAccess(unittest.TestCase):
    def test_sibling_uniform_write_is_visible_only_in_its_own_pass(self):
        with tempfile.TemporaryDirectory(prefix='lwe-material-owner-') as directory:
            root = Path(directory)
            scene, project = write_fixture(root)
            frame, _ = render_scene(self, root, scene, project=project)
            self.assertEqual(frame.getpixel((80,90)), (255,0,0))
            self.assertAlmostEqual(frame.getpixel((240,90))[1], 128, delta=1)
            self.assertEqual(frame.getpixel((240,90))[::2], (0,0))


if __name__ == '__main__':
    unittest.main()
