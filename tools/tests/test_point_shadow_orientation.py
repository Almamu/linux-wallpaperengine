"""Six point-shadow faces, edge filtering, and paired cube-seam samples."""
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest

from scene_render import render_scene
from shadow_depth import prepare


FACE_NAMES = ('+X', '-X', '+Y', '-Y', '+Z', '-Z')
FACE_BASES = (
    ((1, 0, 0), (0, 0, 1), (0, 1, 0)),
    ((-1, 0, 0), (0, 0, -1), (0, 1, 0)),
    ((0, 1, 0), (1, 0, 0), (0, 0, 1)),
    ((0, -1, 0), (1, 0, 0), (0, 0, -1)),
    ((0, 0, 1), (-1, 0, 0), (0, 1, 0)),
    ((0, 0, -1), (1, 0, 0), (0, 1, 0)),
)
SEAMS = tuple((a, b, sa, sb) for a, b in ((0, 1), (0, 2), (1, 2))
              for sa, sb in ((1, 1), (1, -1), (-1, 1), (-1, -1)))
# Native DXVK: identical in all 30 captured frames, medium shadows.
# build/parity-deep-sea/native-point-orientation-control-verification.json
NATIVE_ROWS = [
    [0, 0, 0, 6, 57, 118, 179, 230, 255, 255, 255, 255, 0, 255, 0],
    [0, 0, 0, 6, 57, 118, 179, 230, 255, 255, 255, 255, 0, 255, 0],
    [0, 0, 0, 0, 30, 84, 142, 203, 251, 255, 255, 255, 0, 255, 0],
    [0, 0, 0, 0, 30, 84, 142, 203, 251, 255, 255, 255, 0, 255, 0],
    [0, 0, 0, 0, 5, 53, 114, 173, 226, 255, 255, 255, 0, 255, 0],
    [0, 0, 0, 0, 5, 53, 114, 173, 226, 255, 255, 255, 0, 255, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 255, 0, 0, 0, 255, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 255, 0, 0, 0, 255, 0, 0],
]


def face_position(face, depth, right, up):
    forward, horizontal, vertical = FACE_BASES[face]
    return tuple(depth * forward[i] + right * horizontal[i] + up * vertical[i]
                 for i in range(3))


def seam_position(seam, side):
    a, b, sa, sb = SEAMS[seam]
    position = [0.0, 0.0, 0.0]
    position[a] = sa * (20 if side == 0 else 19.6)
    position[b] = sb * (19.6 if side == 0 else 20)
    return tuple(position)


def prepare_point_orientation(root, assets):
    root, assets = Path(root), Path(assets)
    scene = prepare(root, assets, 10)
    scene['general']['lightconfig'] = {'point': 1, 'pointshadow': 1}
    scene['objects'] = [scene['objects'][0], *scene['objects'][2:]]
    scene['objects'][0].update(radius=100, lightsourcesize=1)
    scene['objects'][1].update(name='Six asymmetric face casters', origin='0 0 0')
    vertices, indices = bytearray(), []
    for face in range(6):
        # At 85 pixels and a 94 degree face, the upper edge is about 37.071 pixels.
        # It stays away from both integer and half-integer raster coordinates.
        for right, up in ((-12, -12), (12, -12), (12, 1.37), (-12, 1.37)):
            position = face_position(face, 10, right, up)
            normal = tuple(-axis for axis in FACE_BASES[face][0])
            vertices.extend(struct.pack('<12f', *position, *normal, 1, 0, 0, 1, 0, 0))
        indices.extend(face * 4 + index for index in (0, 1, 2, 0, 2, 3))
    index_data = struct.pack('<36H', *indices)
    (root / 'models/caster.mdl').write_bytes(b'MDLV0013\0' + struct.pack('<3I', 15, 1, 1)
        + b'materials/caster.json\0' + struct.pack('<2I', 0, len(vertices)) + vertices
        + struct.pack('<I', len(index_data)) + index_data)

    seam_code = []
    for seam in range(12):
        first = ', '.join(f'{value:.8f}' for value in seam_position(seam, 0))
        second = ', '.join(f'{value:.8f}' for value in seam_position(seam, 1))
        seam_code.append(f'    if (seam == {seam}) return side == 0 ? vec3({first}) : vec3({second});')
    stock = (assets / 'shaders/genericimage4.frag').read_text()
    (root / 'shaders/genericimage4.frag').write_text(stock[:stock.index('void main()')] + '''
vec3 facePosition(int face, float right, float up) {
    if (face == 0) return vec3(20.0, up, right);
    if (face == 1) return vec3(-20.0, up, -right);
    if (face == 2) return vec3(right, 20.0, up);
    if (face == 3) return vec3(right, -20.0, -up);
    if (face == 4) return vec3(-right, up, 20.0);
    return vec3(right, up, -20.0);
}
vec3 seamPosition(int seam, int side) {
''' + '\n'.join(seam_code) + '''
    return vec3(20.0, 0.0, 0.0);
}
float generatedShadow(vec3 position) {
    vec3 delta = g_LPoint_Origin[0].xyz - position;
    vec3 normal = normalize(delta);
    vec3 light = PerformLighting_V1(position, CAST3(1.0), normal, normal,
        CAST3(0.0), CAST3(0.0), 1.0, 0.0);
    vec3 unshadowed = ComputePBRLightShadow(normal, delta, normal, CAST3(1.0),
        g_LPoint_Color[0].rgb, g_LPoint_Color[0].w, g_LPoint_Origin[0].w,
        CAST3(0.0), CAST3(0.0), 1.0, 0.0, 1.0);
    return light.r / max(unshadowed.r, 0.00001);
}
float stockShadow(vec3 position) {
    vec4 projected = CalculateProjectedCoordsPoint(position, g_LPoint_Origin[0].xyz,
        g_LFeature_ShadowPointProjection[0], g_LFeature_ShadowPointProjectionTransform[0]);
#if GLSL && !REVERSEDEPTH
    projected.z = projected.z * 0.5 + 0.5 - 0.0015;
#endif
    return PerformPointShadowMapping(projected);
}
void main() {
    int column = int(floor(v_TexCoord.x * 16.0));
    int row = int(floor(v_TexCoord.y * 8.0));
    vec3 position;
    if (row < 6) {
        float up = 2.77 + 0.30 * (float(column) - 5.0);
        if (column == 11 || column == 13) up = 8.0;
        if (column == 12 || column == 14) up = -6.0;
        position = facePosition(row, 0.0, up);
    } else {
        position = column < 12 ? seamPosition(column, row - 6)
            : facePosition(0, 0.0, column == 12 ? 8.0 : -6.0);
    }
    float value = row < 6 && column >= 13 ? stockShadow(position) : generatedShadow(position);
    // The ordered sentinel column also identifies vertical presentation flips.
    gl_FragColor = column == 15 ? vec4(float(row + 1) / 8.0, 0.0, 1.0, 1.0)
        : vec4(CAST3(value), 1.0);
}
''')
    (root / 'scene.json').write_text(json.dumps(scene))
    return scene


def sample_point_orientation(image):
    image = image.convert('RGB')
    if image.size != (320, 180):
        raise ValueError(f'Expected 320x180 probe, got {image.size}')
    rows = [[image.getpixel((int((column + .5) * 20), int((row + .5) * 22.5)))
             for column in range(16)] for row in range(8)]
    candidates = []
    for flipped, candidate in ((False, rows), (True, rows[::-1])):
        error = max(max(abs(channel - expected) for channel, expected in
                        zip(candidate[row][15], (round((row + 1) * 255 / 8), 0, 255)))
                    for row in range(8))
        candidates.append((error, flipped, candidate))
    error, flipped, rows = min(candidates, key=lambda candidate: candidate[0])
    if error > 1:
        raise ValueError(f'Point orientation panel is not ready: sentinel error {error}')
    if any(max(pixel) - min(pixel) > 1 for row in rows for pixel in row[:15]):
        raise ValueError('A shadow-ratio probe was not grayscale')
    return {'rows_flipped': flipped, 'rows': [[pixel[0] for pixel in row[:15]] for row in rows]}


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class PointShadowOrientation(unittest.TestCase):
    def test_six_face_edges_and_cube_seams_match_native(self):
        assets = Path(os.environ.get('LWE_TEST_ASSETS', str(Path.home() /
            '.local/share/Steam/steamapps/common/wallpaper_engine/assets')))
        with tempfile.TemporaryDirectory(prefix='lwe-point-shadow-orientation-') as directory:
            root = Path(directory)
            scene = prepare_point_orientation(root, assets)
            image, output = render_scene(self, root, scene, frames=40, fps=30,
                extra_args=('--msaa', 'off', '--post-processing', 'disabled',
                            '--assets-dir', str(assets)))
            observed = sample_point_orientation(image)['rows']
            for row, (actual, expected) in enumerate(zip(observed, NATIVE_ROWS)):
                for column, (value, reference) in enumerate(zip(actual, expected)):
                    with self.subTest(row=row, column=column):
                        self.assertLessEqual(abs(value - reference), 2, output)
            self.assertNotIn('Failed to setup', output)


if __name__ == '__main__':
    unittest.main()
