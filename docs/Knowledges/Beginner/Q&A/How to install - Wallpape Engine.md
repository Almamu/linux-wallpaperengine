# Buy and install: Wallpaper Engine

You **must own and install Wallpaper Engine** via Steam.

Install the Wallpaper Engine, this provides the required assets used by many backgrounds.

## 1. **Install Steam**

You can choose in your environment:

- From official Steampowered:
	1. You can download steam in here: https://store.steampowered.com/about/
	2. If you ended download, install the package
- Debian & Ubuntu
	1. You can install in one step command: `sudo apt install steam`
- Arch Linux
	1. In Arch Linux, Steam install is need little step, please see this ArchWiki:
	- English: https://wiki.archlinux.org/title/Steam
	- Japanese: https://wiki.archlinux.jp/index.php/Steam
- Fedora
- Please see official document:
	- https://docs.fedoraproject.org/en-US/gaming/proton/

## 2. **Install the Wallpaper Engine**

1. Buy in: https://store.steampowered.com/app/431960/Wallpaper_Engine/
2. Install in Steam app, you can discover in Library, maybe you need to remove search conditions

## 3. Get Wallpaper Engine Assets

### [📰Good News!!] **You usally don't need to copy anything manually.** The app will automatically look in these common install paths:

```
~/.steam/steam/steamapps/common
~/.local/share/Steam/steamapps/common
~/.var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps/common
~/snap/steam/common/.local/share/Steam/steamapps/common
```

> ✅ If Wallpaper Engine is installed in one of these paths, the assets will be detected automatically!

### ⚠️ If Assets Aren’t Found Automatically

You can copy the `assets` folder manually:

1. In Steam, right-click **Wallpaper Engine** → **Manage** → **Browse local files**
2. Copy the `assets` folder
3. Paste it into the same folder where the `linux-wallpaperengine` binary is located