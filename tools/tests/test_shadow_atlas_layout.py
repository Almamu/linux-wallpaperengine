"""Native medium shadows use one 256-square atlas region per shadow feature."""
import copy
import json
import os
from pathlib import Path
import tempfile
import unittest

from scene_render import render_scene
from shadow_depth import decode, prepare


def prepare_atlas_probe(root, assets, points, spots, extra_expressions=()):
    root = Path(root)
    scene = prepare(root, assets, 9990)
    point, spot, caster, receiver = scene['objects']
    lights = []
    # Deliberately author spots before points: native reserves point regions first.
    for template, count in ((spot, spots), (point, points)):
        for _ in range(count):
            light = copy.deepcopy(template)
            light['id'] = 10 + len(lights)
            lights.append(light)
    scene['objects'] = lights + [caster, receiver]
    scene['general']['lightconfig'] = {
        'point': points, 'pointshadow': points, 'spot': spots, 'spotshadow': spots}
    expressions = ['SHADOW_ATLAS_TEXEL.z', 'SHADOW_ATLAS_TEXEL.w']
    for family, count in (('ShadowPointProjectionTransform', points),
                          ('ShadowProjectionTransform', spots)):
        for index in range(count):
            transform = f'g_LFeature_{family}[{index}]'
            expressions.extend(f'{transform}.{component} * SHADOW_ATLAS_TEXEL.{dimension}'
                               for component, dimension in zip('xyzw', 'zwzw'))
    expressions.extend(extra_expressions)
    cells = len(expressions) + 2
    branches = '\n'.join(f'    if (cell == {index + 1}) value = ({expression}) / 8192.0;'
                         for index, expression in enumerate(expressions))
    stock = (Path(assets) / 'shaders/genericimage4.frag').read_text()
    (root / 'shaders/genericimage4.frag').write_text(stock[:stock.index('void main()')] + '''
vec3 encodeValue(float value) {
    float packed = floor(clamp((value + 2.0) * 0.25, 0.0, 1.0) * 16777215.0);
    float red = floor(packed / 65536.0);
    float green = floor((packed - red * 65536.0) / 256.0);
    return vec3(red, green, packed - red * 65536.0 - green * 256.0) / 255.0;
}
void main() {
''' + f'    int cell = int(floor(v_TexCoord.x * {cells}.0));\n'
        + '    float value = 0.123456;\n' + branches + '''
    gl_FragColor = vec4(encodeValue(value), 1.0);
''' + f'    if (cell == {cells - 1})\n' + '''
        gl_FragColor = vec4(CAST3(texSample2DCompare(SHADOW_ATLAS_SAMPLER, vec2(0.5, 0.5), 1.0).r), 1.0);
}
''')
    (root / 'scene.json').write_text(json.dumps(scene))
    return scene


def sample_atlas_probe(image, feature_count, extra_count=0):
    cells = 4 + feature_count * 4 + extra_count
    values = [decode(image.getpixel((int((index + .5) * image.width / cells), image.height // 2)))
              for index in range(cells - 1)]
    return values[0], [value * 8192 for value in values[1:]]


def prepare_visibility_probe(root, assets, cycle):
    scene = prepare_atlas_probe(root, assets, 1, 2)
    for light in scene['objects'][:2]:
        light['parent'] = 30
    visibility = {'value': False, 'script':
        'let ticks=0;export function update(v){++ticks;return ticks>=15 && ticks<30;}'} if cycle else False
    scene['objects'].append({'id': 30, 'name': 'Shadow visibility parent',
                             'solid': True, 'visible': visibility})
    (Path(root) / 'scene.json').write_text(json.dumps(scene))
    return scene


def prepare_camera_order_probe(root, assets, eye_z):
    root, assets = Path(root), Path(assets)
    scene = prepare_atlas_probe(root, assets, 0, 2)
    scene['camera']['eye'] = f'0 0 {eye_z}'
    scene['objects'][0].update(origin='0 0 -2', color='1 0 0')
    scene['objects'][1].update(origin='0 0 2', color='0 0 1')
    vertex = (assets / 'shaders/genericimage4.vert').read_text()
    end = vertex.rfind('}')
    (root / 'shaders/genericimage4.vert').write_text(vertex[:end] + '''
    v_TexCoord = a_TexCoord;
    gl_Position = vec4(a_TexCoord * 2.0 - 1.0, 0.0, 1.0);
''' + vertex[end:])
    path = root / 'shaders/genericimage4.frag'
    fragment = path.read_text()
    path.write_text(fragment[:fragment.index('void main()')] + '''
void main() {
    int cell = int(floor(v_TexCoord.x * 5.0));
    vec4 first = g_LFeature_ShadowProjectionTransform[0];
    vec4 second = g_LFeature_ShadowProjectionTransform[1];
    float redX = g_LSpot_Color[0].r > 0.5 ? first.x : second.x;
    float blueX = g_LSpot_Color[0].r > 0.5 ? second.x : first.x;
    float value = 0.123456;
    if (cell == 1) value = redX * SHADOW_ATLAS_TEXEL.z / 8192.0;
    if (cell == 2) value = blueX * SHADOW_ATLAS_TEXEL.z / 8192.0;
    if (cell == 3) value = SHADOW_ATLAS_TEXEL.z / 8192.0;
    gl_FragColor = vec4(encodeValue(value), 1.0);
    if (cell == 4)
        gl_FragColor = vec4(CAST3(texSample2DCompare(SHADOW_ATLAS_SAMPLER, vec2(0.5, 0.5), 1.0).r), 1.0);
}
''')
    (root / 'scene.json').write_text(json.dumps(scene))
    return scene


def prepare_tied_order_probe(root, assets, hidden):
    scene = prepare_camera_order_probe(root, assets, 12)
    scene['objects'][0]['origin'] = '0 0 0'
    scene['objects'][1]['origin'] = {'value': '0 0 2', 'script':
        'let ticks=0;export function update(v){return ++ticks<15 ? new Vec3(0,0,2) : new Vec3(0,0,0);}'}
    if hidden:
        scene['objects'][1]['visible'] = {'value': False, 'script':
            'let ticks=0;export function update(v){return ++ticks>=20;}'}
    (Path(root) / 'scene.json').write_text(json.dumps(scene))
    return scene


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class ShadowAtlasLayout(unittest.TestCase):
    def test_medium_shadow_features_use_native_regions(self):
        assets = Path(os.environ.get('LWE_TEST_ASSETS', Path.home() /
            '.local/share/Steam/steamapps/common/wallpaper_engine/assets'))
        # Native FUN140190c80 and the DXVK frame37 capture: square features,
        # point regions first, then spots, no guard inset or square-atlas padding.
        cases = (
            ('spot', 0, 1, [256, 256, 0, 0, 256, 256]),
            ('point', 1, 0, [256, 256, 0, 0, 256, 256]),
            ('point-two-spots', 1, 2,
             [768, 256, 0, 0, 256, 256, 256, 0, 256, 256, 512, 0, 256, 256]),
            ('two-points-two-spots', 2, 2,
             [1024, 256, 0, 0, 256, 256, 256, 0, 256, 256,
              512, 0, 256, 256, 768, 0, 256, 256]),
        )
        with tempfile.TemporaryDirectory(prefix='lwe-shadow-atlas-') as directory:
            for name, points, spots, expected in cases:
                with self.subTest(layout=name):
                    root = Path(directory) / name
                    scene = prepare_atlas_probe(root, assets, points, spots)
                    image, output = render_scene(self, root, scene, frames=20,
                        extra_args=('--msaa', 'off', '--post-processing', 'disabled',
                                    '--assets-dir', str(assets)))
                    sentinel, actual = sample_atlas_probe(image, points + spots)
                    self.assertNotIn('GLSL fragment unit parsing Failed', output)
                    self.assertAlmostEqual(sentinel, .123456, delta=1e-6)
                    self.assertEqual(image.getpixel((image.width - 4, image.height // 2)), (255, 255, 255))
                    for index, (value, wanted) in enumerate(zip(actual, expected)):
                        self.assertAlmostEqual(value, wanted, delta=.01,
                            msg=f'{name} component {index}: {actual}')

    def test_hidden_parent_regions_are_removed_without_shrinking_the_atlas(self):
        assets = Path(os.environ.get('LWE_TEST_ASSETS', Path.home() /
            '.local/share/Steam/steamapps/common/wallpaper_engine/assets'))
        with tempfile.TemporaryDirectory(prefix='lwe-shadow-atlas-visibility-') as directory:
            for cycle, expected in ((False, [256, 256, 0, 0, 256, 256]),
                                    (True, [768, 256, 0, 0, 256, 256])):
                with self.subTest(cycle=cycle):
                    root = Path(directory) / ('cycle' if cycle else 'hidden')
                    scene = prepare_visibility_probe(root, assets, cycle)
                    image, output = render_scene(self, root, scene, frames=40, fps=30,
                        extra_args=('--msaa', 'off', '--post-processing', 'disabled', '--assets-dir', str(assets)))
                    sentinel, actual = sample_atlas_probe(image, 3)
                    self.assertNotIn('Failed to setup', output)
                    self.assertAlmostEqual(sentinel, .123456, delta=1e-6)
                    for value, wanted in zip(actual, [*expected, *([0] * 8)], strict=True):
                        self.assertAlmostEqual(value, wanted, delta=.01, msg=str(actual))

    def test_features_wrap_to_the_next_atlas_row(self):
        assets = Path(os.environ.get('LWE_TEST_ASSETS', Path.home() /
            '.local/share/Steam/steamapps/common/wallpaper_engine/assets'))
        with tempfile.TemporaryDirectory(prefix='lwe-shadow-atlas-wrap-') as directory:
            root = Path(directory)
            scene = prepare_atlas_probe(root, assets, 2, 31)
            image, output = render_scene(self, root, scene, frames=20,
                extra_args=('--msaa', 'off', '--post-processing', 'disabled', '--assets-dir', str(assets)))
            sentinel, actual = sample_atlas_probe(image, 33)
            self.assertAlmostEqual(sentinel, .123456, delta=1e-6, msg=output)
            self.assertAlmostEqual(actual[0], 8192, delta=.01)
            self.assertAlmostEqual(actual[1], 512, delta=.01)
            for value, wanted in zip(actual[-8:], [7936, 0, 256, 256, 0, 256, 256, 256], strict=True):
                self.assertAlmostEqual(value, wanted, delta=.01)

    def test_removed_authored_lights_leave_empty_slots(self):
        assets = Path(os.environ.get('LWE_TEST_ASSETS', Path.home() /
            '.local/share/Steam/steamapps/common/wallpaper_engine/assets'))
        expressions = ('g_LPoint_ShadowEnabled[0]', 'g_LSpot_ShadowEnabled[0]',
                       'g_LSpot_ShadowEnabled[1]', 'g_LPoint_Color[0].r',
                       'g_LSpot_Color[0].r', 'g_LSpot_Color[1].b')
        script = '''
let ticks = 0;
export function update(value) {
    if (++ticks === 16) {
        const spot = thisScene.getLayer('Removed spot');
        const point = thisScene.getLayer('Removed point');
        if (!thisScene.destroyLayer(spot) || !thisScene.destroyLayer(point))
            throw Error('Could not queue authored light deletion');
    }
    if (ticks === 18) {
        for (const type of ['lspot', 'lpoint']) {
            if (!thisScene.createLayer({name: type, light: type, castshadow: true, solid: true}))
                throw Error('Could not create replacement light');
        }
    }
    return ticks >= 12;
}
'''
        with tempfile.TemporaryDirectory(prefix='lwe-shadow-atlas-removal-') as directory:
            root = Path(directory)
            scene = prepare_atlas_probe(root, assets, 1, 2, expressions)
            first, remaining, point, _, panel = scene['objects']
            first.update(name='Removed spot', color='1 0 0')
            remaining.update(name='Remaining spot', color='0 0 1')
            point.update(name='Removed point')
            panel['visible']['script'] = script
            image, output = render_scene(self, root, scene, frames=40, fps=30,
                extra_args=('--msaa', 'off', '--post-processing', 'disabled', '--assets-dir', str(assets)))
            sentinel, actual = sample_atlas_probe(image, 3, len(expressions))
            self.assertAlmostEqual(sentinel, .123456, delta=1e-6, msg=output)
            expected = [768, 256, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 256, 256,
                        0, 0, 1, 0, 0, 1]
            for value, wanted in zip(actual, expected, strict=True):
                self.assertAlmostEqual(value, wanted, delta=.01, msg=str(actual))

    def test_camera_forward_orders_physical_regions_without_swapping_light_identity(self):
        assets = Path(os.environ.get('LWE_TEST_ASSETS', Path.home() /
            '.local/share/Steam/steamapps/common/wallpaper_engine/assets'))
        with tempfile.TemporaryDirectory(prefix='lwe-shadow-atlas-camera-') as directory:
            for eye_z, expected in ((12, [256, 0, 512]), (-12, [0, 256, 512])):
                with self.subTest(eye_z=eye_z):
                    root = Path(directory) / str(eye_z)
                    scene = prepare_camera_order_probe(root, assets, eye_z)
                    image, output = render_scene(self, root, scene, frames=20,
                        extra_args=('--msaa', 'off', '--post-processing', 'disabled', '--assets-dir', str(assets)))
                    self.assertNotIn('Failed to setup', output)
                    self.assertAlmostEqual(decode(image.getpixel((32, 90))), .123456, delta=1e-6)
                    for x, wanted in zip((96, 160, 224), expected):
                        self.assertAlmostEqual(decode(image.getpixel((x, 90))) * 8192, wanted, delta=.01)
                    self.assertEqual(image.getpixel((288, 90)), (255, 255, 255))

    def test_equal_depth_retains_prior_order_including_hidden_lights(self):
        assets = Path(os.environ.get('LWE_TEST_ASSETS', Path.home() /
            '.local/share/Steam/steamapps/common/wallpaper_engine/assets'))
        with tempfile.TemporaryDirectory(prefix='lwe-shadow-atlas-ties-') as directory:
            for hidden in (False, True):
                with self.subTest(hidden=hidden):
                    root = Path(directory) / ('hidden' if hidden else 'visible')
                    scene = prepare_tied_order_probe(root, assets, hidden)
                    image, output = render_scene(self, root, scene, frames=40, fps=30,
                        extra_args=('--msaa', 'off', '--post-processing', 'disabled', '--assets-dir', str(assets)))
                    self.assertNotIn('Failed to setup', output)
                    self.assertAlmostEqual(decode(image.getpixel((32, 90))), .123456, delta=1e-6)
                    red_x = decode(image.getpixel((96, 90))) * 8192
                    blue_x = decode(image.getpixel((160, 90))) * 8192
                    self.assertGreater(red_x, blue_x, 'Equal depth must retain the previous blue/red order')
                    self.assertAlmostEqual(red_x, 256, delta=.01)
                    self.assertAlmostEqual(blue_x, 0, delta=.01)


if __name__ == '__main__':
    unittest.main()
