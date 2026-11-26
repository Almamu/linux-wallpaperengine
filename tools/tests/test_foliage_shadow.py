"""Stock foliage deformation and cutout coverage must also reach the shadow atlas."""
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest

from scene_render import render_scene
from shadow_depth import prepare


def prepare_foliage(root, assets, blending, leaves_mode=None):
    scene = prepare(root, assets, 1000)
    stock = (assets / 'shaders/genericimage4.frag').read_text()
    fragment = stock[:stock.index('void main()')] + """
float pointCompare(vec3 position) {
    vec4 atlas = g_LFeature_ShadowPointProjectionTransform[0];
    vec4 p = CalculateProjectedCoordsPoint(position, g_LPoint_Origin[0].xyz,
        g_LFeature_ShadowPointProjection[0], atlas);
#if GLSL
#if !REVERSEDEPTH
    p.z = p.z * 0.5 + 0.5 - 0.0015;
#endif
#endif
    return texSample2DCompare(SHADOW_ATLAS_SAMPLER, p.xy, p.z).r;
}
float spotCompare(vec3 position) {
    vec3 p = CalculateProjectedCoords(position, g_LFeature_ShadowProjection[0]);
    vec4 atlas = g_LFeature_ShadowProjectionTransform[0];
#if GLSL
#if !REVERSEDEPTH
    p.z = p.z * 0.5 + 0.5 - 0.0015;
#endif
#endif
    return texSample2DCompare(SHADOW_ATLAS_SAMPLER, p.xy * atlas.zw + atlas.xy, p.z).r;
}
void main() {
    int cell = int(floor(v_TexCoord.x * 6.0));
    int probe = cell % 3;
    vec3 position = probe == 0 ? vec3(0.0, 600.0, 2000.0)
        : vec3(probe == 1 ? 500.0 : 700.0, 1200.0, 2000.0);
    float value = cell < 3 ? pointCompare(position) : spotCompare(position);
    gl_FragColor = vec4(CAST3(value), 1.0);
}
"""
    (root / 'shaders/genericimage4.frag').write_text(fragment)
    pixels = bytes((255, 255, 255, 0, 255, 255, 255, 255)) * 2
    (root / 'materials/cutout.tex').write_bytes(
        b'TEXV0005\0TEXI0001\0' + struct.pack('<7I', 0, 3, 2, 2, 2, 2, 0)
        + b'TEXB0001\0' + struct.pack('<5I', 1, 1, 2, 2, len(pixels)) + pixels)
    material = {'passes': [{
        'shader': 'foliage4', 'textures': ['cutout'], 'blending': blending,
        'combos': {'LEAVESUVMODE': 1},
        # In the stock helper, these values make every vertex move +300 in X/Y.
        # This is independent of simulation time and uses the real foliage source.
        'constantshadervalues': {'foliageheight': 1, 'strengthbase': 150,
            'strengthleaves': 0, 'foliagescale': 0, 'foliagespeedbase': 0,
            'foliagespeedleaves': 0, 'foliagephase': 1.5707963267948966},
        'depthwrite': 'enabled', 'depthtest': 'enabled', 'cullmode': 'nocull'}]}
    if leaves_mode is not None:
        material['passes'][0]['combos'] = {'leavesuvmode': leaves_mode}
        material['passes'][0]['constantshadervalues'].update(strengthbase=0, strengthleaves=150)
        # Both X endpoints have sin(world.x) > -.3, fully enabling the leaf gate.
        scene['objects'][2]['origin'] = '1.5707963267948966 0 -8990'
        (root / 'scene.json').write_text(json.dumps(scene))
    (root / 'materials/caster.json').write_text(json.dumps(material))
    vertices = b''.join(struct.pack('<12f', x, y, 9990.0, 0, 0, -1, 1, 0, 0, 1,
                                   1 if leaves_mode is not None else u, 1 if leaves_mode is not None else v)
        for x, y, u, v in ((-100, 200, 0, 0), (100, 200, 1, 0),
                          (100, 400, 1, 1), (-100, 400, 0, 1)))
    indices = struct.pack('<6H', 0, 1, 2, 0, 2, 3)
    (root / 'models/caster.mdl').write_bytes(b'MDLV0013\0' + struct.pack('<3I', 15, 1, 1)
        + b'materials/caster.json\0' + struct.pack('<2I', 0, len(vertices)) + vertices
        + struct.pack('<I', len(indices)) + indices)
    return scene


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class FoliageShadow(unittest.TestCase):
    def test_foliage_shadow_uses_deformed_position_and_material_coverage(self):
        assets = Path(os.environ.get('LWE_TEST_ASSETS', str(Path.home() /
            '.local/share/Steam/steamapps/common/wallpaper_engine/assets')))
        for blending in ('normal', 'alphatocoverage'):
            with self.subTest(blending=blending), tempfile.TemporaryDirectory(prefix='lwe-foliage-shadow-') as directory:
                root = Path(directory)
                scene = prepare_foliage(root, assets, blending)
                image, output = render_scene(self, root, scene, frames=40, fps=30,
                    extra_args=('--msaa', 'off', '--post-processing', 'disabled',
                                '--assets-dir', str(assets)))
                white, black = (255, 255, 255), (0, 0, 0)
                expected = [white, white if blending == 'alphatocoverage' else black, black] * 2
                # Old position is clear; displaced left/right halves obey alpha.
                self.assertEqual([image.getpixel((round((i + .5) * 320 / 6), 90)) for i in range(6)],
                                 expected, output)
                self.assertNotIn('Failed to setup', output)

    def test_leaf_uv_mode_controls_shadow_displacement(self):
        assets = Path(os.environ.get('LWE_TEST_ASSETS', str(Path.home() /
            '.local/share/Steam/steamapps/common/wallpaper_engine/assets')))
        white, black = (255, 255, 255), (0, 0, 0)
        for mode, expected in ((-1, [white, black, black] * 2), (0, [white, black, black] * 2),
                               (1, [black, white, white] * 2)):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory(prefix='lwe-foliage-uv-shadow-') as directory:
                root = Path(directory)
                scene = prepare_foliage(root, assets, 'normal', leaves_mode=mode)
                image, output = render_scene(self, root, scene, frames=40, fps=30,
                    extra_args=('--msaa', 'off', '--post-processing', 'disabled', '--assets-dir', str(assets)))
                self.assertEqual([image.getpixel((round((i + .5) * 320 / 6), 90)) for i in range(6)],
                                 expected, output)
                self.assertNotIn('Failed to setup', output)

    def test_custom_foliage_filename_without_wind_parameters_still_casts(self):
        assets = Path(os.environ.get('LWE_TEST_ASSETS', str(Path.home() /
            '.local/share/Steam/steamapps/common/wallpaper_engine/assets')))
        with tempfile.TemporaryDirectory(prefix='lwe-custom-foliage-shadow-') as directory:
            root = Path(directory)
            scene = prepare_foliage(root, assets, 'normal')
            (root / 'shaders/foliage4.vert').write_text("""
attribute vec3 a_Position;
uniform mat4 g_ModelViewProjectionMatrix;
void main() { gl_Position = mul(vec4(a_Position, 1.0), g_ModelViewProjectionMatrix); }
""")
            (root / 'shaders/foliage4.frag').write_text("void main() { gl_FragColor = vec4(1.0); }")
            image, output = render_scene(self, root, scene, frames=40, fps=30,
                extra_args=('--msaa', 'off', '--post-processing', 'disabled', '--assets-dir', str(assets)))
            white, black = (255, 255, 255), (0, 0, 0)
            self.assertEqual([image.getpixel((round((i + .5) * 320 / 6), 90)) for i in range(6)],
                             [black, white, white] * 2, output)
            self.assertNotIn('Failed to setup', output)


if __name__ == '__main__':
    unittest.main()
