"""Effect visibility callbacks distinguish their own settings from the layer and other effects."""
import copy
import json
from pathlib import Path
import tempfile
import unittest

from scene_render import render_scene


SCRIPT = '''
let frames = 0;
export function update() {
    if (++frames !== 3) return;
    const before = [thisObject.visible, thisLayer.visible];
    ACTION
    const after = [thisObject.visible, thisLayer.visible];
    const material = thisObject.getMaterial?.(0);
    const animation = material?.getAnimation('pulse');
    const exactEffect = thisLayer.getEffect('Primary');
    console.log('EFFECT_OWNER ' + JSON.stringify({
        mode: MODE, before, after,
        matchingEffect: exactEffect.visible === thisObject.visible,
        sibling: thisLayer.getEffect('Sibling')?.visible ?? null,
        materialAnimationRate: animation?.rate ?? null,
        materialFunction: typeof thisObject.executeMaterialFunction,
        visibilityAnimationRate: MODE === 'text-object' ? thisObject.getAnimation?.()?.rate ?? null : null,
    }));
    RETURN
}
'''


def write_effect(root):
    source = {
        'effects/owner-probe/effect.json': json.dumps({
            'passes': [{'material': 'materials/effects/owner-probe.json'}],
        }),
        'materials/effects/owner-probe.json': json.dumps({
            'passes': [{'shader': 'effects/owner-probe', 'blending': 'normal',
                        'depthtest': 'disabled', 'depthwrite': 'disabled', 'cullmode': 'nocull'}],
        }),
        'shaders/effects/owner-probe.vert': (
            'uniform mat4 g_ModelViewProjectionMatrix;\n'
            'attribute vec3 a_Position;\n'
            'void main() { gl_Position = mul(vec4(a_Position, 1.0), g_ModelViewProjectionMatrix); }\n'
        ),
        'shaders/effects/owner-probe.frag': (
            'uniform float g_UserAlpha; // {"material":"alpha","default":1.0}\n'
            'void main() { gl_FragColor = vec4(1.0, 0.0, 0.0, g_UserAlpha); }\n'
        ),
    }
    for name, contents in source.items():
        path = root/name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(contents)


class EffectVisibilityOwner(unittest.TestCase):
    def test_same_effect_is_reached_without_mutating_its_layer_or_siblings(self):
        animation = {'value':1, 'animation': {'options':{'name':'pulse', 'fps':30,'length':30},
                                              'c0':[{'frame':0,'value':1},{'frame':30,'value':1}]}}
        tint = {'file':'effects/owner-probe/effect.json','id':100,'name':'Primary','visible':True,
                'passes':[{'constantshadervalues':{'alpha':animation,'color':'1 0 0'}}]}
        objects = []
        for number,(kind,action) in enumerate((('image-object','thisObject.visible = false;'),
                                               ('image-layer','thisLayer.visible = false;'),
                                               ('image-return',''),('text-object','thisObject.visible = false;'),
                                               ('shape-object','thisObject.visible = false;')),1):
            script = SCRIPT.replace('MODE',json.dumps(kind)).replace('ACTION',action).replace(
                'RETURN','return false;' if kind=='image-return' else '')
            primary = copy.deepcopy(tint)
            primary['visible'] = {'value':True,'script':script}
            obj = {'id':number, 'name':kind, 'origin':f'{50 * number} 90 0','size':'32 32', 'effects':[primary]}
            if kind.startswith('image'):
                obj['image'] = 'models/util/solidlayer.json'
                sibling = copy.deepcopy(tint)
                sibling.update(id=101, name='Sibling')
                obj['effects'].append(sibling)
            elif kind.startswith('text'):
                primary['visible']['animation'] = copy.deepcopy(animation['animation'])
                obj.update(text='control', font='systemfont_arial')
            else:
                obj['shape'] = 'quad'
            objects.append(obj)
        objects.append({'id':20,'name':'reader','solid':True,'visible':{'value':True,'script':'''
            let frames=0;
            export function update(value) {
                if (++frames === 5) {
                    const subject = thisScene.getLayer('image-return');
                    console.log('EFFECT_RETURN ' + JSON.stringify({effect:subject.getEffect('Primary').visible,layer:subject.visible}));
                }
                return value;
            }
        '''}})
        scene = {'camera':{'eye':'0 0 1','center':'0 0 0','up':'0 1 0'},
                 'general':{'orthogonalprojection':{'width':320,'height':180},'clearcolor':'0 0 0'}, 'objects':objects}
        with tempfile.TemporaryDirectory(prefix='lwe-effect-visibility-owner-') as directory:
            root = Path(directory)
            write_effect(root)
            _, output = render_scene(self, root, scene, frames=8)
        entries = [json.loads(entry.split('EFFECT_OWNER ',1)[1]) for entry in output.splitlines() if 'EFFECT_OWNER ' in entry]
        after = {'image-object':[False,True],'image-layer':[True,False],'image-return':[True,True],
                 'text-object':[False,True],'shape-object':[False,True]}
        self.assertEqual({entry['mode']:entry['after'] for entry in entries},after,entries)
        for entry in entries:
            self.assertEqual(entry['before'],[True,True],entry)
            self.assertIs(entry['matchingEffect'],True,entry)
            self.assertIs(entry['sibling'],True if entry['mode'].startswith('image') else None,entry)
            self.assertEqual(entry['materialAnimationRate'],1,entry)
            self.assertEqual(entry['materialFunction'],'function',entry)
            if entry['mode'] == 'text-object':
                self.assertEqual(entry['visibilityAnimationRate'], 1, entry)
        returns = [json.loads(entry.split('EFFECT_RETURN ',1)[1]) for entry in output.splitlines() if 'EFFECT_RETURN ' in entry]
        self.assertEqual(returns,[{'effect':False,'layer':True}],output)


if __name__ == '__main__':
    unittest.main()
