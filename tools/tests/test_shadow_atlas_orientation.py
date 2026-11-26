"""Spot receivers preserve orientation when sampling a point-light atlas tile."""
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest

from scene_render import render_scene
from shadow_depth import prepare


SAMPLE_PIXELS = [(40, 90), (120, 90), (200, 90), (280, 90)]
# Native DXVK, all 30 frames: build/parity-deep-sea/
# native-mixed-shadow-orientation-control-verification.json.
NATIVE_RGB = [(0, 0, 0), (0, 0, 0), (255, 255, 255), (255, 0, 255)]


def prepare_atlas_orientation_probe(root, assets):
    root, assets = Path(root), Path(assets)
    scene = prepare(root, assets, 5)
    # A nonzero blue point light remains active without entering the red ratio.
    scene['objects'][0].update(name='Blue point tile', radius=100,
                               lightsourcesize=0.05, color='0 0 1')
    scene['objects'][1].update(name='Red spot receiver light', radius=100,
                               innercone=20, outercone=30, scale='2 2 2', color='1 0 0')
    scene['objects'][2].update(name='Point minus X caster', origin='0 0 0',
                               angles='0 0 0', scale='1 1 1')
    # Only the -X point face contains this opaque, two-sided plane. Its footprint
    # comfortably covers the PCF kernel but stays outside the +Z spot projection.
    vertices = b''.join(struct.pack('<12f', -5, y, z, 1, 0, 0, 0, 1, 0, 1, 0, 0)
        for y, z in ((-0.5, -0.5), (0.5, -0.5), (0.5, 0.5), (-0.5, 0.5)))
    indices = struct.pack('<6H', 0, 1, 2, 0, 2, 3)
    (root / 'models/caster.mdl').write_bytes(b'MDLV0013\0' + struct.pack('<3I', 15, 1, 1)
        + b'materials/caster.json\0' + struct.pack('<2I', 0, len(vertices)) + vertices
        + struct.pack('<I', len(indices)) + indices)

    stock = (assets / 'shaders/genericimage4.frag').read_text()
    (root / 'shaders/genericimage4.frag').write_text(stock[:stock.index('void main()')] + '''
vec3 pointTileReceiver() {
    vec4 pointTile = g_LFeature_ShadowPointProjectionTransform[0];
    vec4 spotTile = g_LFeature_ShadowProjectionTransform[0];
    vec2 target = pointTile.xy + vec2(0.75, 1.0 / 6.0) * pointTile.zw;
    vec2 projected = (target - spotTile.xy) / spotTile.zw;
    // +Z spot: right=-X, up=+Y. Invert its native UV at depth 20 without any
    // receiver-side Y correction. The target lies outside the spot's own tile.
    return vec3((vec2(0.5, 0.5) - projected) * (40.0 * 0.5773502691896258), 20.0);
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
    int cell = int(floor(v_TexCoord.x * 4.0));
    vec3 position = cell < 2 ? pointTileReceiver() : vec3(0.0, 0.0, 20.0);
    float value = cell == 1 ? directShadow(position) : generatedShadow(position);
    gl_FragColor = cell == 3 ? vec4(1.0, 0.0, 1.0, 1.0) : vec4(CAST3(value), 1.0);
}
''')
    (root / 'scene.json').write_text(json.dumps(scene))
    (root / 'project.json').write_text(json.dumps({'type': 'scene', 'file': 'scene.json',
        'title': 'Mixed shadow atlas orientation'}))
    return scene


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class ShadowAtlasOrientation(unittest.TestCase):
    def test_spot_receiver_keeps_point_tile_orientation(self):
        assets = Path(os.environ.get('LWE_TEST_ASSETS', str(Path.home() /
            '.local/share/Steam/steamapps/common/wallpaper_engine/assets')))
        with tempfile.TemporaryDirectory(prefix='lwe-shadow-atlas-orientation-') as directory:
            root = Path(directory)
            scene = prepare_atlas_orientation_probe(root, assets)
            image, output = render_scene(self, root, scene, frames=40, fps=30,
                extra_args=('--msaa', 'off', '--post-processing', 'disabled',
                            '--assets-dir', str(assets)))
            # Generated cross-tile lookup, direct stock lookup, clear inside,
            # readiness. A spot-local Y flip sends only the first lookup to -Z.
            self.assertEqual([image.getpixel(pixel) for pixel in SAMPLE_PIXELS],
                             NATIVE_RGB, output)
            self.assertNotIn('Failed to setup', output)
            self.assertNotIn('GLSL fragment unit parsing Failed', output)


if __name__ == '__main__':
    unittest.main()
