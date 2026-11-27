"""A retained effect handle must not keep the owning renderer alive."""
import json
from pathlib import Path
import tempfile
import unittest

from scene_render import render_scene
from test_effect_visibility_owner import write_effect


SCRIPT = '''
let frame = 0, effect, queued = false, reported = false;
function read(item) {
    return [item.visible ?? null, typeof item.getMaterial(0)];
}
export function update(value) {
    if (++frame === 1) {
        const layer = thisScene.createLayer({name:'effect owner', image:'models/util/solidlayer.json',
            size:'30 30', effects:[{ id:77, name:'Ephemeral',file:'effects/owner-probe/effect.json',
                visible:{value:true, script:'export function init() { shared.savedThisObject = thisObject; }'},
                passes:[{constantshadervalues:{alpha:1}}]}]});
        effect = layer.getEffect(0);
    } else if (frame === 3) {
        console.log('HELD_EFFECT_LIVE ' + JSON.stringify([read(effect), read(shared.savedThisObject)]));
        queued = thisScene.destroyLayer('effect owner');
    } else if (queued && !reported && !thisScene.getLayer('effect owner')) {
        reported = true;
        effect.visible = false;
        shared.savedThisObject.visible = false;
        effect.executeMaterialFunction('missing');
        console.log('HELD_EFFECT_GONE ' + JSON.stringify([read(effect), read(shared.savedThisObject)]));
    }
    return value;
}
'''


class EffectLifetime(unittest.TestCase):
    def test_invalidated_layer(self):
        scene = {'camera':{'eye':'0 0 1','center':'0 0 0','up':'0 1 0'},
                 'general':{'orthogonalprojection':{'width':320,'height':180},'clearcolor':'0 0 0'},
                 'objects':[{'id':1,'name':'driver','solid':True,'visible':{'value':True,'script':SCRIPT}}]}
        with tempfile.TemporaryDirectory(prefix='lwe-effect-lifetime-') as directory:
            root = Path(directory)
            write_effect(root)
            _, output = render_scene(self, root, scene, frames=11)
        for state, expected in (('LIVE',[[True,'object'],[True,'object']]),
                                ('GONE',[[None,'undefined'],[None,'undefined']])):
            marker = f'HELD_EFFECT_{state} '
            lines = [json.loads(line.split(marker,1)[1]) for line in output.splitlines() if marker in line]
            self.assertEqual(lines,[expected],output)


if __name__ == '__main__':
    unittest.main()
