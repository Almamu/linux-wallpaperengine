"""Property-script callbacks resolve runtime-layer assets in their workshop scope."""
import json
import os
from pathlib import Path
import tempfile
import unittest

from scene_render import render_scene


def script(label, declaration, hook='init'):
    body = '''
let reported = false;
export function HOOK(value) {
    if (reported) return value;
    reported = true;
    function image(path) {
        const layer = thisScene.createLayer(path);
        return layer ? thisScene.getInitialLayerConfig(layer).image : null;
    }
    console.log('WORKSHOP_SCOPE ' + JSON.stringify([LABEL,
        image('models/lwe-workshop-probe.json'), image('models/lwe-direct-probe.json')]));
    return value;
}
'''
    return declaration + '\n' + body.replace('LABEL', json.dumps(label)).replace('HOOK', hook)


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class ScriptWorkshopScope(unittest.TestCase):
    def test_runtime_layer_assets_use_current_module_workshop_id(self):
        direct = 'models/lwe-direct-probe.json'
        scoped = 'models/workshop/2127844565/lwe-workshop-probe.json'
        other = 'models/workshop/2652493753/lwe-workshop-probe.json'
        scripts = [
            script('authored', "export let __workshopId = '2127844565';"),
            script('other', "import*as math from'WEMath';export/* module */const\u00a0__workshopId = "
                            "String(math.mix(2652493752, 2652493754, 0.5));",
                   'update'),
            script('absent', "globalThis.__workshopId = '2127844565';"),
            script('local', "// export let __workshopId = '2652493753';\nlet __workshopId = '2127844565';"),
            script('invalid', "export var __workshopId = '../private';"),
        ]
        scene = {
            'camera': {'eye': '0 0 1', 'center': '0 0 0', 'up': '0 1 0'},
            'general': {'orthogonalprojection': {'width': 320, 'height': 180},
                        'clearcolor': '0 0 0', 'camerafade': False},
            'objects': [{'id': number, 'name': f'module {number}', 'solid': True,
                         'visible': {'value': True, 'script': source}}
                        for number, source in enumerate(scripts, 1)],
        }
        with tempfile.TemporaryDirectory(prefix='lwe-workshop-scope-') as directory:
            root = Path(directory)
            for name in (direct, scoped, other, 'models/workshop/2127844565/lwe-direct-probe.json',
                         'models/private/lwe-workshop-probe.json'):
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(json.dumps({'material': 'materials/util/solidlayer.json', 'solidlayer': True}))
            _, output = render_scene(self, root, scene)

        records = [json.loads(line.split('WORKSHOP_SCOPE ', 1)[1])
                   for line in output.splitlines() if 'WORKSHOP_SCOPE ' in line]
        self.assertCountEqual(records, [
            ['authored', scoped, direct], ['other', other, direct],
            ['absent', None, direct], ['local', None, direct], ['invalid', None, direct],
        ], output)


if __name__ == '__main__':
    unittest.main()
