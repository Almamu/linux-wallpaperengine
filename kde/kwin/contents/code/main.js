// SPDX-License-Identifier: GPL-3.0-or-later
// KWin 6.5+: constrain() remains valid across focus and stacking changes.
const plugin = "io.github.linuxwallpaperengine.desktop";
let excludedWindow = null;
let lastState = "";

function isWallpaper(window) {
    return String(window.resourceClass) === "linux-wallpaperengine";
}

function synchronize() {
    const windows = workspace.stackingOrder.filter(w => w !== excludedWindow);
    const wallpapers = windows.filter(isWallpaper);
    const desktops = windows.filter(w => w.desktopWindow && String(w.resourceClass) === "plasmashell");
    for (const wallpaper of wallpapers) {
        for (const desktop of desktops) {
            workspace.constrain(wallpaper, desktop);
        }
    }
    const state = JSON.stringify(wallpapers.map(w => ({x:w.frameGeometry.x, y:w.frameGeometry.y, pid:w.pid})));
    if (state === lastState) return;
    lastState = state;
    // Only touch desktops explicitly configured for this integration. This also
    // restores the fallback after SIGKILL or a renderer crash.
    const script = 'var positions=' + state + '; var plugin=' + JSON.stringify(plugin) + ';'
        + 'desktops().forEach(function(d) {'
        + 'if(d.screen < 0 || d.wallpaperPlugin !== plugin) return;'
        + 'var g=screenGeometry(d.screen);'
        + 'var active=positions.some(function(p){return p.x===g.x && p.y===g.y;});'
        + 'd.currentConfigGroup=["Wallpaper",plugin,"General"];'
        + 'd.writeConfig("Active",active);'
        + 'var targets=positions.filter(function(p){return p.x===g.x && p.y===g.y && p.pid>0;})'
        + '.map(function(p){return {x:p.x,y:p.y,service:"org.linuxwallpaperengine.Mouse.p"+p.pid};});'
        + 'd.writeConfig("InputTargets",JSON.stringify(targets));'
        + '});';
    callDBus("org.kde.plasmashell", "/PlasmaShell", "org.kde.PlasmaShell", "evaluateScript", script);
}

function added(window) {
    if (isWallpaper(window)) {
        window.frameGeometryChanged.connect(synchronize);
    }
    // A new Plasma desktop may have persisted Active=true or belong to a
    // restarted plasmashell. Force synchronization even with the same renderer.
    lastState = "";
    synchronize();
}
workspace.windowAdded.connect(added);
workspace.windowRemoved.connect(function(window) {
    excludedWindow = window;
    lastState = "";
    synchronize();
    excludedWindow = null;
});
workspace.currentActivityChanged.connect(function() { lastState = ""; synchronize(); });
for (const window of workspace.stackingOrder) {
    if (isWallpaper(window)) window.frameGeometryChanged.connect(synchronize);
}
synchronize();
