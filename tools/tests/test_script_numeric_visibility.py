"""A numeric SceneScript return must not overwrite Boolean layer visibility."""
import json
import os
from pathlib import Path
import tempfile
import unittest

from scene_render import render_scene


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics tests')
class ScriptNumericVisibility(unittest.TestCase):
    def test_numeric_updates_preserve_visibility_and_update_numeric_properties(self):
        settings = {'zero': (True, '0'), 'one': (False, '1'), 'half': (True, '0.5'),
                    'true': (False, 'true'), 'false': (True, 'false'), 'void': (True, 'undefined')}
        objects = [dict(id=i, name=name, text=name, font='systemfont_arial', origin='160 90 0',
                        visible={'value':default,'script':f'export function update() {{ return {result}; }}'})
                   for i,(name,(default,result)) in enumerate(settings.items(),1)]
        objects[0]['pointsize'] = {'value':27,'script':'export function update() { return 42.5; }'}
        objects.append(dict(id=10,name='reader',solid=True,visible={'value':True,'script':'''let frames = 0;
            export function update(value) {
                if (++frames === 4) {
                    const layers = ['zero', 'one', 'half', 'true', 'false', 'void'];
                    console.log('LWE_VISIBILITY ' + JSON.stringify({
                        visibility: layers.map(name => thisScene.getLayer(name).visible),
                        pointsize: thisScene.getLayer('zero').pointsize
                    }));
                }
                return value;
            }'''}))
        scene = dict(camera={'eye':'0 0 1','center':'0 0 0','up':'0 1 0'},
                     general={'orthogonalprojection':{'width':320,'height':180},'clearcolor':'0 0 0'},
                     objects=objects)
        with tempfile.TemporaryDirectory(prefix='lwe-number-visibility-') as directory:
            _, output = render_scene(self, Path(directory), scene, frames=10)
        lines = [line.split('LWE_VISIBILITY ',1)[1] for line in output.splitlines() if 'LWE_VISIBILITY ' in line]
        self.assertEqual(len(lines),1,output)
        self.assertEqual(json.loads(lines[0]),dict(visibility=[True,False,True,True,False,True],pointsize=42.5))
        self.assertTrue(all(type(value) is bool for value in json.loads(lines[0])['visibility']),lines[0])


if __name__ == '__main__':
    unittest.main()
