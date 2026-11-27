"""A rejected particle material keeps a valid child emitter branch."""
import json
import os
from pathlib import Path
import tempfile
import unittest

import numpy as np

from scene_render import render_scene


VERTEX = """
attribute vec3 a_Position;
attribute vec4 a_TexCoordVec4;
uniform mat4 g_ModelViewProjectionMatrix;
void main() {
    vec3 position = a_Position + vec3((a_TexCoordVec4.xy - 0.5) * 28.0, 0.0);
    gl_Position = mul(vec4(position, 1.0), g_ModelViewProjectionMatrix);
}
"""


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class ParticleFailedMaterial(unittest.TestCase):
    def render_probe(self, failure, child_kind):
        with tempfile.TemporaryDirectory(prefix='lwe-particle-dropped-pass-') as directory:
            root = Path(directory)
            for folder in ('particles', 'materials', 'shaders'):
                (root / folder).mkdir()
            parent_declares_child = child_kind != 'none'
            for obj, shader in [('parent', failure), ('child', 'green')]:
                pass_definition = {'shader': shader, 'textures': ['util/white'], 'blending': 'normal',
                                   'depthtest': 'disabled', 'depthwrite': 'disabled', 'cullmode': 'nocull'}
                (root / f'materials/{obj}.json').write_text(json.dumps({'passes': [pass_definition]}))
                particle = {'maxcount': 1, 'material': f'materials/{obj}.json',
                            'emitter': [{'name': 'sphererandom', 'instantaneous': 1,
                                         'rate': 0, 'distancemin': 0, 'distancemax': 0}],
                            'initializer': [{'name': 'lifetimerandom', 'min': 8, 'max': 8}],
                            'renderer': [{'name': 'sprite'}]}
                if obj == 'parent' and parent_declares_child:
                    path = 'particles/nonexistent.json' if child_kind == 'missing' else 'particles/child.json'
                    particle['children'] = [{'name': path, 'type': child_kind if child_kind != 'missing' else 'static',
                                             'maxcount': 1, 'probability': 1}]
                (root / f'particles/{obj}.json').write_text(json.dumps(particle))
            for shader in ('rejected', 'green'):
                (root / f'shaders/{shader}.vert').write_text(VERTEX)
                color = 'undefinedParticleColor()' if shader == 'rejected' else 'vec4(0.0, 1.0, 0.0, 1.0)'
                (root / f'shaders/{shader}.frag').write_text(f'void main() {{\n    gl_FragColor = {color};\n}}\n')
            scene = {'camera': {'eye': '160 90 100', 'center': '160 90 0', 'up': '0 1 0'},
                     'general': {'orthogonalprojection': {'width': 320, 'height': 180},
                                 'clearcolor': '0 0 0', 'bloom': False},
                     'objects': [{'id': 1, 'name': 'Parent', 'origin': '160 90 0',
                                  'particle': 'particles/parent.json'}]}
            image, log = render_scene(self, root, scene, frames=12, fps=24)
            red, green, blue = np.asarray(image).transpose(2, 0, 1)
            pixels = int(np.count_nonzero((green > 160) & (red < 16) & (blue < 16)))
            return pixels, log

    def test_translated_child_still_draws_after_parent_material_rejection(self):
        for kind in ('static', 'eventfollow'):
            with self.subTest(child=kind):
                pixels, log = self.render_probe('rejected', kind)
                self.assertIn('GLSL fragment unit parsing Failed', log)
                self.assertGreater(pixels, 200, log)
                self.assertNotIn('Failed to setup object 1', log)

    def test_missing_parent_shader_has_its_original_failure(self):
        pixels, log = self.render_probe('missing', 'static')
        self.assertEqual(pixels, 0)
        self.assertIn('Failed to setup object 1', log)

    def test_a_rejected_shader_without_a_usable_child_still_fails(self):
        for kind in ('none', 'missing'):
            with self.subTest(child=kind):
                pixels, log = self.render_probe('rejected', kind)
                self.assertEqual(pixels, 0)
                self.assertIn('Failed to setup object 1', log)


if __name__ == '__main__':
    unittest.main()
