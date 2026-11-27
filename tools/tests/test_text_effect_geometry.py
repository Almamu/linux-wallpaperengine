"""Text position shaders use their effect surface's pixel coordinate system."""
import json
import os
from pathlib import Path
import tempfile
import unittest

from scene_render import render_scene


def write_copy_assets(root):
    for subdirectory in ('materials', 'shaders', 'effects'):
        (root / subdirectory).mkdir(parents=True)
    for name, shift in (('copy', '0.0'), ('displace', '12.0'),
                        ('local', 'abs(a_Position.x) > 1.0 ? 10000.0 : 0.0')):
        (root / f'shaders/{name}.vert').write_text('''
attribute vec3 a_Position;
attribute vec2 a_TexCoord;
uniform mat4 g_ModelViewProjectionMatrix;
varying vec2 v_TexCoord;
void main() {
    v_TexCoord = a_TexCoord;
    gl_Position = mul(vec4(a_Position + vec3(SHIFT, 0.0, 0.0), 1.0), g_ModelViewProjectionMatrix);
}
'''.replace('SHIFT', str(shift)))
        (root / f'shaders/{name}.frag').write_text('''
uniform sampler2D g_Texture0;
varying vec2 v_TexCoord;
void main() { gl_FragColor = texSample2D(g_Texture0, v_TexCoord); }
''')
        (root / f'materials/{name}.json').write_text(json.dumps({'passes': [{
            'shader': name, 'blending': 'normal', 'depthtest': 'disabled',
            'depthwrite': 'disabled', 'cullmode': 'nocull'}]}))


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class TextEffectGeometry(unittest.TestCase):
    def test_vertex_effect_positions_use_text_surface_pixels(self):
        with tempfile.TemporaryDirectory(prefix='lwe-text-effect-position-') as directory:
            bounds = {}
            for label in ('plain', 'copy', 'displaced', 'named-targets', 'hidden-trailing'):
                with self.subTest(label=label):
                    root = Path(directory) / label
                    write_copy_assets(root)
                    text = {'id': 1, 'name': 'Test glyph', 'origin': '160 90 0',
                            'pointsize': 12, 'padding': '32 32', 'text': 'H', 'color': '1 1 1', 'alpha': 1}
                    if label != 'plain':
                        passes = [{'material': ('materials/copy.json' if label == 'copy'
                                                else 'materials/displace.json')}]
                        fbos = []
                        if label == 'named-targets':
                            # Named intermediates keep their NDC geometry even when the
                            # terminal untargeted pass draws in scene coordinates.
                            fbos = [{'name': target, 'scale': 1.0, 'format': 'rgba8888'}
                                    for target in ('_rt_first', '_rt_second')]
                            passes = [{'material': 'materials/local.json', 'target': '_rt_first'},
                                      {'material': 'materials/local.json', 'target': '_rt_second'},
                                      {'material': 'materials/displace.json'}]
                        (root / 'effects/geometry.json').write_text(json.dumps({'passes': passes, 'fbos': fbos}))
                        text['effects'] = [{'id': 2, 'file': 'effects/geometry.json'}]
                        if label == 'hidden-trailing':
                            (root / 'effects/hidden.json').write_text(json.dumps({
                                'passes': [{'material': 'materials/copy.json'}]}))
                            text['effects'].append({'id': 3, 'file': 'effects/hidden.json', 'visible': False})
                    scene = {
                        'camera': {'eye': '0 0 0', 'center': '0 0 -1', 'up': '0 1 0'},
                        'general': {'orthogonalprojection': {'width': 320, 'height': 180},
                                    'clearcolor': '0 0 0', 'camerafade': False},
                        'objects': [text],
                    }
                    frame, _ = render_scene(self, root, scene, extra_args=('--msaa', 'off'))
                    bounds[label] = frame.getchannel('R').point(lambda level: 255 if level >= 128 else 0).getbbox()
            self.assertIsNotNone(bounds.get('plain'), 'Control glyph must draw')
            self.assertIsNotNone(bounds.get('copy'), 'Identity effect glyph must draw')
            for direct, effected in zip(bounds['plain'], bounds['copy']):
                self.assertLessEqual(abs(direct - effected), 1)
            shifted = tuple(value + 12 if i % 2 == 0 else value for i, value in enumerate(bounds['copy']))
            for label in ('displaced', 'named-targets', 'hidden-trailing'):
                with self.subTest(label=label):
                    self.assertEqual(bounds.get(label), shifted)

    def test_texture_consumers_and_copy_commands_keep_local_geometry(self):
        with tempfile.TemporaryDirectory(prefix='lwe-text-effect-local-') as directory:
            for label in ('retained', 'color-blend', 'scene-bind', 'trailing-copy'):
                with self.subTest(label=label):
                    root = Path(directory) / label
                    write_copy_assets(root)
                    effect = {'passes': [{'material': 'materials/local.json'}]}
                    objects = [{'id': 1, 'name': 'Source glyph', 'origin': '80 90 0',
                                'pointsize': 12, 'padding': '32 32', 'text': 'H',
                                'color': '1 1 1', 'alpha': 1,
                                'effects': [{'id': 2, 'file': 'effects/geometry.json'}]}]
                    if label == 'retained':
                        (root / 'models').mkdir()
                        (root / 'models/view.json').write_text(json.dumps({'material': 'materials/view.json'}))
                        material = json.loads((root / 'materials/copy.json').read_text())
                        material['passes'][0].update(textures=['_rt_imageLayerComposite_1_a'],
                                                    blending='translucent')
                        (root / 'materials/view.json').write_text(json.dumps(material))
                        objects.append({'id': 3, 'name': 'Retained glyph', 'image': 'models/view.json',
                                        'origin': '240 90 0', 'size': '96 96', 'dependencies': [1]})
                    elif label == 'color-blend':
                        objects[0]['colorBlendMode'] = 9
                    elif label == 'scene-bind':
                        (root / 'shaders/scene.vert').write_bytes((root / 'shaders/local.vert').read_bytes())
                        (root / 'shaders/scene.frag').write_text('''
uniform sampler2D g_Texture0;
uniform sampler2D g_Texture1;
varying vec2 v_TexCoord;
void main() {
    vec4 glyph = texSample2D(g_Texture0, v_TexCoord);
    float red = texSample2D(g_Texture1, vec2(0.1, 0.1)).g * 2.0;
    gl_FragColor = vec4(red, glyph.gb, glyph.a);
}
''')
                        (root / 'materials/scene.json').write_text(
                            (root / 'materials/local.json').read_text().replace('"local"', '"scene"'))
                        effect['passes'][0] = {'material': 'materials/scene.json',
                                               'bind': [{'index': 1, 'name': '_rt_FullFrameBuffer'}]}
                    else:
                        effect['fbos'] = [{'name': target, 'scale': 1.0, 'format': 'rgba8888'}
                                          for target in ('_rt_source', '_rt_saved')]
                        effect['passes'] = [
                            {'material': 'materials/local.json', 'target': '_rt_source'},
                            {'material': 'materials/local.json'},
                            {'command': 'copy', 'source': '_rt_source', 'target': '_rt_saved'},
                        ]
                    (root / 'effects/geometry.json').write_text(json.dumps(effect))
                    scene = {
                        'camera': {'eye': '0 0 0', 'center': '0 0 -1', 'up': '0 1 0'},
                        'general': {'orthogonalprojection': {'width': 320, 'height': 180},
                                    'clearcolor': '0 0 0', 'camerafade': False},
                        'objects': objects,
                    }
                    if label == 'scene-bind':
                        scene['general']['clearcolor'] = '0 0.5 0'
                    frame, output = render_scene(
                        self, root, scene, extra_args=('--msaa', 'off', '--render-debug', 'pass-log'))
                    red = frame.getchannel('R')
                    self.assertIsNotNone(red.crop((24, 24, 132, 150)).getbbox())
                    if label == 'trailing-copy':
                        self.assertIn('shader=commands/copy target=_rt_saved drawTo=_rt_saved', output)
                    if label == 'retained':
                        self.assertIsNotNone(frame.crop((184, 24, 292, 150)).getbbox())
