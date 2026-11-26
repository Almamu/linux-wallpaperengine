"""Opt-in alpha-cutout shadow binding regression; root owns the isolated renderer."""
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest

from scene_render import render_scene
from shadow_depth import prepare


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class ShadowAlphaToCoverage(unittest.TestCase):
    def test_coverage_caster_samples_its_albedo_alpha(self):
        assets = Path(os.environ.get('LWE_TEST_ASSETS', str(Path.home() /
            '.local/share/Steam/steamapps/common/wallpaper_engine/assets')))
        with tempfile.TemporaryDirectory(prefix='lwe-shadow-a2c-') as directory:
            root = Path(directory)
            scene = prepare(root, assets, 100)
            stock = (assets / 'shaders/genericimage4.frag').read_text()
            fragment = stock[:stock.index('void main()')] + """
float pointCompare(vec3 position) {
    vec4 projected = CalculateProjectedCoordsPoint(position, g_LPoint_Origin[0].xyz,
        g_LFeature_ShadowPointProjection[0], g_LFeature_ShadowPointProjectionTransform[0]);
    return texSample2DCompare(SHADOW_ATLAS_SAMPLER, projected.xy, projected.z).r;
}
float spotCompare(vec3 position) {
    vec3 projected = CalculateProjectedCoords(position, g_LFeature_ShadowProjection[0]);
    vec4 atlas = g_LFeature_ShadowProjectionTransform[0];
    return texSample2DCompare(SHADOW_ATLAS_SAMPLER,
        projected.xy * atlas.zw + atlas.xy, projected.z).r;
}
void main() {
    int cell = int(floor(v_TexCoord.x * 4.0));
    vec3 position = vec3((cell % 2 == 0) ? -2500.0 : 2500.0, 0.0, 9990.0);
    float value = cell < 2 ? pointCompare(position) : spotCompare(position);
    gl_FragColor = vec4(vec3(value), 1.0);
}
"""
            (root / 'shaders/genericimage4.frag').write_text(fragment)
            # A literal nearest-sampled two-column mask: left transparent, right opaque.
            pixels = bytes((255, 255, 255, 0, 255, 255, 255, 255)) * 2
            (root / 'materials/cutout.tex').write_bytes(
                b'TEXV0005\0TEXI0001\0' + struct.pack('<7I', 0, 3, 2, 2, 2, 2, 0)
                + b'TEXB0001\0' + struct.pack('<5I', 1, 1, 2, 2, len(pixels)) + pixels)
            (root / 'materials/caster.json').write_text(json.dumps({'passes': [{
                'shader': 'generic4', 'textures': ['cutout'], 'blending': 'alphatocoverage',
                'depthwrite': 'enabled', 'depthtest': 'enabled', 'cullmode': 'nocull'}]}))
            vertices = b''.join(struct.pack('<12f', x, y, 9990.0, 0, 0, -1, 1, 0, 0, 1, u, v)
                for x, y, u, v in ((-2000, -2000, 0, 0), (2000, -2000, 1, 0),
                                  (2000, 2000, 1, 1), (-2000, 2000, 0, 1)))
            indices = struct.pack('<6H', 0, 1, 2, 0, 2, 3)
            (root / 'models/caster.mdl').write_bytes(b'MDLV0013\0' + struct.pack('<3I', 15, 1, 1)
                + b'materials/caster.json\0' + struct.pack('<2I', 0, len(vertices)) + vertices
                + struct.pack('<I', len(indices)) + indices)
            image, output = render_scene(self, root, scene, frames=40, fps=30,
                extra_args=('--msaa', 'off', '--post-processing', 'disabled',
                            '--assets-dir', str(assets)))
            # Each light must see through the left half and be blocked by the right.
            self.assertEqual([image.getpixel((x, 90)) for x in (40, 120, 200, 280)],
                             [(255, 255, 255), (0, 0, 0), (255, 255, 255), (0, 0, 0)], output)
            self.assertNotIn('Failed to setup', output)


if __name__ == '__main__':
    unittest.main()
