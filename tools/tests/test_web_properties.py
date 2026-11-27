"""Opt-in regression for the property payload delivered to web wallpapers."""
import json
import os
from pathlib import Path
import re
import tempfile
import unittest
from unittest.mock import patch

from scene_render import render_scene


@unittest.skipUnless(os.environ.get('LWE_TEST_BINARY'), 'Set LWE_TEST_BINARY for graphics integration tests')
class WebProperties(unittest.TestCase):
    def test_typed_values_reach_late_listener_on_startup_and_reload(self):
        project = {
            'type': 'web', 'file': 'index.html', 'title': 'Web property regression',
            'general': {'properties': {
                'saturation': {'type': 'slider', 'value': 90, 'min': 1, 'max': 100,
                               'order': 12, 'editable': False, 'condition': 'projection.value'},
                'projection': {'type': 'bool', 'value': True},
                'color': {'type': 'color', 'value': '0.25 0.5 1'},
                'mode': {'type': 'combo', 'value': False, 'text': 'Display mode', 'options': [
                    {'label': 'Off', 'value': False}, {'label': 'On', 'value': True}]},
                'texture': {'type': 'combo', 'value': 0, 'options': [
                    {'label': 'Default', 'value': 0}, {'label': 'Second', 'value': 2}]},
                'text_mode': {'type': 'combo', 'value': '2', 'options': [{'label': 'Second', 'value': '2'}]},
                'background': {'type': 'file'},
                'info': {'type': 'text', 'text': 'Choose a setting'},
                'group': {'type': 'group', 'text': 'Options'},
            }},
        }
        with tempfile.TemporaryDirectory(prefix='lwe-web-') as directory:
            root = Path(directory)
            (root / 'index.html').write_text('''<!doctype html>
<script>
setTimeout(() => {
    window.wallpaperPropertyListener = {
        applyUserProperties(properties) {
            console.log('WEB_PROPERTIES:' + JSON.stringify(properties));
            if (!sessionStorage.reloaded) {
                sessionStorage.reloaded = '1';
                location.reload();
            }
        }
    };
}, 250);
</script>
''')
            cef_flags = os.environ.get('WPE_CEF_EXTRA', '') + ' --enable-logging=stderr --log-level=0'
            with patch.dict(os.environ, {'WPE_CEF_EXTRA': cef_flags, 'TMPDIR': directory}):
                _, log = render_scene(self, root, {}, frames=30, project=project,
                                      extra_args=('--set-property', 'saturation=77',
                                                  '--set-property', 'texture=2'))

        callbacks = [json.loads(payload) for payload in re.findall(r'WEB_PROPERTIES:(\{.*\})', log)]
        self.assertEqual(len(callbacks), 2, log)
        for properties in callbacks:
            self.assertEqual({key: item.get('type') for key, item in properties.items()}, {
                'saturation': 'slider', 'projection': 'bool', 'color': 'color',
                'mode': 'combo', 'texture': 'combo', 'text_mode': 'combo',
                'background': 'file', 'info': 'text',
            })
            self.assertEqual(properties['saturation'], {
                'type': 'slider', 'value': 77, 'min': 1, 'max': 100,
                'order': 12, 'editable': False, 'condition': 'projection.value',
            })
            self.assertIs(properties['projection']['value'], True)
            self.assertEqual(properties['color']['value'], '0.25 0.5 1')
            self.assertIs(properties['mode']['value'], False)
            self.assertEqual(properties['mode']['text'], 'Display mode')
            self.assertEqual(properties['mode']['options'], [
                {'label': 'Off', 'value': False}, {'label': 'On', 'value': True}])
            self.assertEqual(properties['texture']['value'], 2)
            self.assertEqual(properties['text_mode']['value'], '2')
            self.assertEqual(properties['background'], {'type': 'file'})
            self.assertEqual(properties['info'], {'type': 'text', 'text': 'Choose a setting'})


if __name__ == '__main__':
    unittest.main()
