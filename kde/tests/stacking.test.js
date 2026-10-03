// Run with: node kde/tests/stacking.test.js
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../kwin/contents/code/main.js'), 'utf8');
const plugin = 'io.github.linuxwallpaperengine.desktop';
function signal() {
    const callbacks = [];
    return {connect: fn => callbacks.push(fn), emit: (...args) => callbacks.forEach(fn => fn(...args))};
}
const configs = [{Active: false}, {Active: false}, {Active: false}];
const desktops = [0, 1, -1].map((screen, i) => ({screen, wallpaperPlugin: i === 1 ? 'org.kde.image' : plugin, writeConfig: (k,v) => configs[i][k] = v}));
const shell = () => ({resourceClass:'plasmashell', desktopWindow:true});
let nextPid = 100;
const renderer = (x=0,y=294) => ({pid: nextPid++,resourceClass:'linux-wallpaperengine', desktopWindow:false, frameGeometry:{x,y}, frameGeometryChanged:signal()});
const desktop = shell();
const wallpaper = renderer();
const constraints = [];
const workspace = {stackingOrder:[desktop], windowAdded:signal(), windowRemoved:signal(), currentActivityChanged:signal(), constrain:(a,b) => constraints.push([a,b])};
let available = true;
vm.runInNewContext(source, {workspace, callDBus: (service, object, iface, method, script) => {
    if (available) vm.runInNewContext(script, {desktops:() => desktops, screenGeometry:s => s===0?{x:0,y:294}:{x:2560,y:0}});
}});
assert.equal(configs[0].Active, false);
workspace.stackingOrder.push(wallpaper);
workspace.windowAdded.emit(wallpaper);
assert.equal(configs[0].Active, true);
assert.equal(JSON.parse(configs[0].InputTargets)[0].service, 'org.linuxwallpaperengine.Mouse.p' + wallpaper.pid);
assert(constraints.some(([a,b]) => a===wallpaper && b===desktop));
assert.equal(configs[1].Active, false, 'Other wallpaper plugins must not be changed');
assert.equal(configs[2].Active, false, 'Disconnected containments must not be changed');
workspace.windowRemoved.emit(wallpaper); // KWin may still include it in stackingOrder here.
workspace.stackingOrder = [desktop];
assert.equal(configs[0].Active, false, 'Removal/crash restores fallback');
assert.equal(configs[0].InputTargets, '[]');
const second = renderer();
workspace.stackingOrder.push(wallpaper, second);
workspace.windowAdded.emit(second);
workspace.windowRemoved.emit(wallpaper);
workspace.stackingOrder = [desktop, second];
assert.equal(configs[0].Active, true, 'One remaining renderer keeps the wallpaper active');
assert.deepEqual(JSON.parse(configs[0].InputTargets).map(t=>t.service), ['org.linuxwallpaperengine.Mouse.p'+second.pid]);
second.frameGeometry = {x:2560,y:0};
second.frameGeometryChanged.emit();
assert.equal(configs[0].Active, false, 'Moving output updates fallback');
available=false;
second.frameGeometry = {x:0,y:294};
second.frameGeometryChanged.emit();
available=true;
workspace.windowAdded.emit(shell());
assert.equal(configs[0].Active, true, 'A failed D-Bus call must not deadlock synchronization after Plasma restarts');
console.log('PASS: stacking, output matching, removal/crash, multiple renderers and Plasma restart');
