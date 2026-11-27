"""Borrowed SceneScript vectors report their actual components and prototype methods."""
import json
import os
from pathlib import Path
import tempfile
import unittest

from scene_render import render_scene


SCRIPT = '''
export let scriptProperties = createScriptProperties().finish();
export function update(value) {
    if (!globalThis.testVectorComponentsReported) {
        globalThis.testVectorComponentsReported = true;
        const vectors = [scriptProperties.xy, scriptProperties.xyz, scriptProperties.rgba];
        const rgba = vectors[2];
        Object.defineProperty(rgba, 'explicitUndefined', {value: undefined});
        console.log('LWE_VECTOR_MEMBERSHIP ' + JSON.stringify({
            members: vectors.map(vector =>
                ['x', 'y', 'z', 'w', 'r', 'absent', 'copy'].map(name => name in vector)),
            explicitUndefined: 'explicitUndefined' in rgba && rgba.explicitUndefined === undefined,
            rgbaComponentsMatchCopy: rgba.copy().x === rgba.x && rgba.copy().w === rgba.w,
        }));
    }
    return value;
}
'''


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class ScriptVectorMembership(unittest.TestCase):
    def test_borrowed_membership(self):
        scene = {
            'camera': {'eye': '0 0 1', 'center': '0 0 0', 'up': '0 1 0'},
            'general': {'orthogonalprojection': {'width': 320, 'height': 180}, 'clearcolor': '0 0 0'},
            'objects': [{'id': 1, 'name': 'probe', 'solid': True, 'visible': {
                'value': True, 'script': SCRIPT,
                'scriptproperties': {'xy': '0.25 0.5', 'xyz': '0.25 0.5 0.75', 'rgba': '0.25 0.5 0.75 1'},
            }}],
        }
        with tempfile.TemporaryDirectory(prefix='lwe-vector-membership-') as directory:
            _, output = render_scene(self, Path(directory), scene, frames=6)
        lines = [line.split('LWE_VECTOR_MEMBERSHIP ', 1)[1]
                 for line in output.splitlines() if 'LWE_VECTOR_MEMBERSHIP ' in line]
        self.assertEqual(len(lines), 1, output)
        self.assertEqual(json.loads(lines[0]), {
            'members': [
                [True, True, False, False, False, False, True],
                [True, True, True, False, False, False, True],
                [True, True, True, True, False, False, True],
            ],
            'explicitUndefined': True,
            'rgbaComponentsMatchCopy': True,
        })


if __name__ == '__main__':
    unittest.main()
