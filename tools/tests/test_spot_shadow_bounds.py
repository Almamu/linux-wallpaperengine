"""Spot receivers sample adjacent atlas tiles even outside their own projection."""
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest

from scene_render import render_scene
from shadow_depth import prepare


def prepare_bounds_probe(root, assets):
    root, assets = Path(root), Path(assets)
    scene = prepare(root, assets, 5)
    scene['general']['lightconfig'] = {'spot': 2, 'spotshadow': 2}
    spot = dict(scene['objects'][1], radius=100, innercone=20, outercone=30,
                scale='2 2 2')
    scene['objects'][:2] = [dict(spot, id=1, name='Red receiver light', color='1 0 0'),
                            dict(spot, id=2, name='Blue adjacent tile light', color='0 0 1')]
    scene['objects'][2].update(name='Center tile caster', origin='0 0 0')
    vertices = b''.join(struct.pack('<12f', x, y, 5, 0, 0, -1, 1, 0, 0, 1, 0, 0)
        for x, y in ((-1, -1), (1, -1), (1, 1), (-1, 1)))
    indices = struct.pack('<6H', 0, 1, 2, 0, 2, 3)
    (root / 'models/caster.mdl').write_bytes(b'MDLV0013\0' + struct.pack('<3I', 15, 1, 1)
        + b'materials/caster.json\0' + struct.pack('<2I', 0, len(vertices)) + vertices
        + struct.pack('<I', len(indices)) + indices)

    stock = (assets / 'shaders/genericimage4.frag').read_text()
    (root / 'shaders/genericimage4.frag').write_text(stock[:stock.index('void main()')] + '''
vec3 adjacentTileReceiver() {
    vec4 first = g_LFeature_ShadowProjectionTransform[0];
    vec4 second = g_LFeature_ShadowProjectionTransform[1];
    vec2 projected = (second.xy + second.zw * 0.5 - first.xy) / first.zw;
    // Both lights point +Z, with right=-X and up=+Y. Invert their 30 degree cone.
    // Adjacent tile centers produce about 49 degrees and distance 15.3 < radius 100.
    // Raw direction length 2 keeps this receiver fully inside the lighting cone.
    return vec3((vec2(0.5, 0.5) - projected) * (20.0 * 0.5773502691896258), 10.0);
}
float directShadow(vec3 position) {
    vec3 projected = CalculateProjectedCoords(position, g_LFeature_ShadowProjection[0]);
#if GLSL && !REVERSEDEPTH
    projected.z = projected.z * 0.5 + 0.5 - 0.0015;
#endif
    return PerformShadowMapping(projected, g_LFeature_ShadowProjectionTransform[0]);
}
float generatedShadow(vec3 position) {
    vec3 delta = g_LSpot_Origin[0].xyz - position;
    vec3 normal = normalize(delta);
    vec3 light = PerformLighting_V1(position, CAST3(1.0), normal, normal,
        CAST3(0.0), CAST3(0.0), 1.0, 0.0);
    float cookie = smoothstep(g_LSpot_Direction[0].w, g_LSpot_Origin[0].w,
        -dot(normal, g_LSpot_Direction[0].xyz));
    vec3 unshadowed = ComputePBRLightShadow(normal, delta, normal, CAST3(1.0),
        g_LSpot_Color[0].rgb * cookie, g_LSpot_Color[0].w, g_LSpot_Exponent[0].x,
        CAST3(0.0), CAST3(0.0), 1.0, 0.0, 1.0);
    return light.r / max(unshadowed.r, 0.00001);
}
void main() {
    int cell = int(floor(v_TexCoord.x * 5.0));
    vec3 position = cell < 2 ? adjacentTileReceiver()
        : vec3(cell == 2 ? -4.0 : 0.0, 0.0, 10.0);
    float value = cell == 1 ? directShadow(position) : generatedShadow(position);
    gl_FragColor = cell == 4 ? vec4(1.0, 0.0, 1.0, 1.0) : vec4(CAST3(value), 1.0);
}
''')
    (root / 'scene.json').write_text(json.dumps(scene))
    return scene


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class SpotShadowBounds(unittest.TestCase):
    def test_outside_projection_receiver_samples_adjacent_tile(self):
        assets = Path(os.environ.get('LWE_TEST_ASSETS', str(Path.home() /
            '.local/share/Steam/steamapps/common/wallpaper_engine/assets')))
        with tempfile.TemporaryDirectory(prefix='lwe-spot-shadow-bounds-') as directory:
            root = Path(directory)
            scene = prepare_bounds_probe(root, assets)
            image, output = render_scene(self, root, scene, frames=40, fps=30,
                extra_args=('--msaa', 'off', '--post-processing', 'disabled',
                            '--assets-dir', str(assets)))
            # Generated outside sample, direct outside sample, clear/blocked inside,
            # then a readiness sentinel. A bounds bypass changes only the first cell.
            self.assertEqual([image.getpixel((x, 90)) for x in (32, 96, 160, 224, 288)],
                [(0, 0, 0), (0, 0, 0), (255, 255, 255), (0, 0, 0), (255, 0, 255)], output)
            self.assertNotIn('Failed to setup', output)
            self.assertNotIn('GLSL fragment unit parsing Failed', output)


if __name__ == '__main__':
    unittest.main()
