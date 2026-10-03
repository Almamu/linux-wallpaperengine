#!/usr/bin/env python3
"""Install the optional KDE Plasma 6.5+ integration for one Plasma desktop."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

PLUGIN = "io.github.linuxwallpaperengine.desktop"
KWIN = "linux-wallpaperengine-desktop"


def run(*args):
    return subprocess.check_output(args, text=True).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--desktop", type=int, help="Plasma containment ID (see --list)")
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--restore", action="store_true", help="Restore the saved wallpaper type")
    parser.add_argument("--mouse-interaction", choices=("on", "off"), help="Enable/disable experimental passive desktop mouse forwarding")
    parser.add_argument("--no-autostart", action="store_true", help="Skip installation of automatic wallpaper restoration")
    args = parser.parse_args()
    qdbus = next((shutil.which(name) for name in ("qdbus-qt6", "qdbus6", "qdbus") if shutil.which(name)), None)
    if not qdbus:
        parser.error("Qt 6 qdbus is required")

    def plasma(script):
        return run(qdbus, "org.kde.plasmashell", "/PlasmaShell", "org.kde.PlasmaShell.evaluateScript", script)

    if args.list:
        print(plasma('print(JSON.stringify(desktops().map(d=>({id:d.id,screen:d.screen,wallpaper:d.wallpaperPlugin,geometry:d.screen>=0?screenGeometry(d.screen):null})),null,2))'))
        return
    if args.desktop is None:
        parser.error("Use --list, then --desktop ID")
    desktops = json.loads(plasma('print(JSON.stringify(desktops().map(d=>({id:d.id,screen:d.screen}))))'))
    if not any(d["id"] == args.desktop and d["screen"] >= 0 for d in desktops):
        parser.error("Desktop is not attached to a connected screen")
    prefix = 'var d=desktopById(' + str(args.desktop) + '); var p=' + json.dumps(PLUGIN) + ';'
    if args.restore:
        print(plasma(prefix + 'if(d.wallpaperPlugin===p){d.currentConfigGroup=["Wallpaper",p,"General"];d.wallpaperPlugin=d.readConfig("PreviousWallpaper","org.kde.image");}'))
        print("Saved wallpaper restored. Its original settings were retained.")
        return

    source = Path(__file__).resolve().parent
    data = Path(os.environ.get("XDG_DATA_HOME", str(Path.home() / ".local/share")))
    wallpaper = data / "plasma/wallpapers" / PLUGIN
    kwin = data / "kwin/scripts" / KWIN
    shutil.copytree(source / "plasma", wallpaper, dirs_exist_ok=True)
    shutil.copytree(source / "kwin", kwin, dirs_exist_ok=True)
    run("kwriteconfig6", "--file", "kwinrc", "--group", "Plugins", "--key", KWIN + "Enabled", "true")
    print(plasma(prefix + '''
if(d.wallpaperPlugin!==p){
  var previous=d.wallpaperPlugin;
  d.currentConfigGroup=["Wallpaper",previous,"General"];
  var color=previous==="org.kde.color"?d.readConfig("Color","#000000"):"#000000";
  var image=previous==="org.kde.image"?d.readConfig("Image",""):"";
  d.wallpaperPlugin=p;
  d.currentConfigGroup=["Wallpaper",p,"General"];
  d.writeConfig("PreviousWallpaper",previous);
  d.writeConfig("FallbackColor",color);
  d.writeConfig("FallbackImage",image);
}
'''))
    if args.mouse_interaction is not None:
        print(plasma(prefix + 'd.currentConfigGroup=["Wallpaper",p,"General"];d.writeConfig("MouseRelayEnabled",' + str(args.mouse_interaction == "on").lower() + ');'))
    run(qdbus, "org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting.unloadScript", KWIN)
    script_id = run(qdbus, "org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting.loadScript", str(kwin / "contents/code/main.js"), KWIN)
    if int(script_id) < 0:
        raise RuntimeError("KWin could not load the integration script")
    run(qdbus, "org.kde.KWin", "/Scripting/Script" + script_id, "org.kde.kwin.Script.run")
    if not args.no_autostart:
        subprocess.run([sys.executable, str(source / "autostart/install.py"), "--if-supported"], check=True)
    print("Installed. Restart Plasma once (or log out/in) to create an alpha-capable desktop:")
    print("  systemctl --user restart plasma-plasmashell.service")
    print("After that, start/stop linux-wallpaperengine normally, including from a GUI.")


if __name__ == "__main__":
    main()
