"""Minimal perspective spot-volume scene shared by the direction and inside tests."""


def prepare_volume_scene(root):
    (root / 'shaders').mkdir(parents=True, exist_ok=True)
    light = {'id': 2, 'name': 'Projected beam', 'light': 'lspot', 'origin': '-4 0 0',
             'angles': '0 0 0', 'radius': 9, 'innercone': 15, 'outercone': 25,
             'color': '1 1 1', 'intensity': 8, 'density': 1,
             'volumetricsexponent': 1, 'castvolumetrics': True, 'castshadow': False,
             'usecookie': False, 'cookie': 'cookie'}
    return {'camera': {'eye': '0 0 12', 'center': '0 0 0', 'up': '0 1 0'},
            'general': {'orthogonalprojection': None, 'nearz': .1, 'farz': 100, 'fov': 45,
                        'ambientcolor': '0 0 0', 'skylightcolor': '0 0 0', 'clearcolor': '0 0 0',
                        'clearenabled': True, 'bloom': False, 'hdr': False, 'camerafade': False,
                        'lightconfig': {'spot': 1, 'spotcookie': 0}},
            'objects': [light, {'id': 3, 'name': 'Camera', 'camera': 'default',
                                'origin': '0 0 12', 'angles': '0 0 0', 'fov': 45}]}
