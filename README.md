<p align="center">
	<a href="https://github.com/Almamu/linux-wallpaperengine/blob/main/LICENSE"><img src="https://img.shields.io/github/license/Almamu/linux-wallpaperengine" /></a>
</p>

# 1. Disclaimer
**This is an educational project**. The project started as a fork from [Almamu/linux-wallpaperengine](https://github.com/Almamu/linux-wallpaperengine) so please check out the original!
For more information on the project's license, check [LICENSE](LICENSE).

# 2. What is this project all about?
This project aims to reproduce the background functionality of Wallpaper Engine on Linux systems. Simple as that. This fork in particular adds a GUI for selecting your wallpapers! 

> ⚠️ This is an educational project that evolved into a functional OpenGL-based wallpaper engine for Linux. Expect some limitations and quirks!

---

## 📦 System Requirements

To compile and run this, you'll need:

- OpenGL 3.3 support
- CMake
- LZ4, Zlib
- SDL2
- FFmpeg
- X11 or Wayland
- Xrandr (for X11)
- GLFW3, GLEW, GLUT, GLM
- MPV
- PulseAudio
- FFTW3
- Qt6

## Commands
```
# Ubuntu
sudo apt-get update
sudo apt-get install build-essential cmake libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev libglew-dev freeglut3-dev libsdl2-dev liblz4-dev libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libxxf86vm-dev libglm-dev libglfw3-dev libmpv-dev mpv libmpv1 libpulse-dev libpulse0 qtbase5-dev

# Fedora
sudo dnf install @development-tools cmake libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel mesa-libGL-devel glew-devel freeglut-devel SDL2-devel lz4-devel libXxf86vm-devel glm-devel glfw-devel mpv-devel qt5-qtbase-devel
```

# 5. How to use
## 5.1. Pre-requirements
In order to properly use this software you'll need to own an actual copy of the Windows version of Wallpaper Engine (which you can buy on the [Steam Page](https://store.steampowered.com/app/431960/Wallpaper_Engine/)), as it contains some basic assets on which most of the backgrounds are based on.

The only way to get those assets is to install the Windows version through Steam. Luckily you don't really need a Windows installation for that. Using the Linux Steam client is enough to download the files we need. Note you may need to check "Enable Steam Play for all other titles" in the Steam Play section of Steam's settings if you haven't already. Also note that the software cannot actually be launched through Steam Play (Proton), but the setting is required for Steam to download the software.

## 5.2. Extracting the assets
The automatic way doesn't require anything extra, as long as Wallpaper Engine is installed in Steam the software should automatically detect where the assets are.

### 5.2.1. Extracting the assets manually
In the off-case where the software doesn't automatically detect the correct path, the assets can be extracted manually. Once Wallpaper Engine is downloaded, open the installation folder (Right-Click the application in Steam -> Manage -> Browse local files). Here you'll see the main folders of Wallpaper Engine. The folder we're interested in is the one named "assets".

![folder](docs/images/screenshot_folder.png)

The assets folder itself **must** be copied to the same folder where the binary lives.

## 5.3. Getting the sources
You can download a zipped version of the repository here: https://github.com/Almamu/linux-wallpaperengine/archive/refs/heads/main.zip

You can also clone the repository using git like this:
```
git clone git@github.com:Deliasama/linux-wallpaperengine.git
```

Or using the HTTPS method if you haven't set up SSH:
```
https://github.com/Deliasama/linux-wallpaperengine.git
```

> This installs the latest development version.

**Note:** You’ll still need assets from the official Wallpaper Engine (via Steam). See below for details.

---

## 🚀 Getting Started

### 1. Get Wallpaper Engine Assets

You **must own and install Wallpaper Engine** via Steam. This provides the required assets used by many backgrounds.

Good news: **you usually don’t need to copy anything manually.** The app will automatically look in these common install paths:

```
~/.steam/steam/steamapps/common
~/.local/share/Steam/steamapps/common
~/.var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps/common
~/snap/steam/common/.local/share/Steam/steamapps/common
```

> ✅ If Wallpaper Engine is installed in one of these paths, the assets will be detected automatically!

---

#### ❗ If Assets Aren’t Found Automatically

You can copy the `assets` folder manually:

1. In Steam, right-click **Wallpaper Engine** → **Manage** → **Browse local files**
2. Copy the `assets` folder
3. Paste it into the same folder where the `linux-wallpaperengine` binary is located

---

### 2. Build from Source

Clone the repo:

```bash
git clone --recurse-submodules https://github.com/Almamu/linux-wallpaperengine.git
cd linux-wallpaperengine
```

Build it:

```bash
mkdir build && cd build
cmake ..
make
```

Once the build process is finished, this should create a new `output` folder containing the app and all the required
support files to run.

## 5.5. Running linux-wallpaperengine
Now you can run the program using the following command:

```
./linux-wallpaperengine
```

## 5.6 Selecting a wallpaper
In the graphical user interface (GUI), start by selecting the desired screen. Then simply click on a wallpaper to display it.

#### Wayland
Has only been tested under wlroots but should work on any flavour as long as wlr-layer-shell-unstable is supported.

#### X11
Only screens configured with the XRandr extension are supported.

**IMPORTANT: Right now this doesn't work if there is anything drawing to the background (like a compositor, gnome, kde, nautilus, etc)**

### 5.6.1 Using custom-flags
To further customize your wallpaper (e.g., adjusting the volume), you can enter one or more of the following flags in the text field at the bottom of the GUI **before** selecting a wallpaper:

| Option | Description |
|--------|-------------|
| `--silent` | Mute background audio |
| `--volume <val>` | Set audio volume |
| `--noautomute` | Don't mute when other apps play audio |
| `--no-audio-processing` | Disable audio reactive features |
| `--fps <val>` | Limit frame rate |
| `--scaling <mode>` | Wallpaper scaling: `stretch`, `fit`, `fill`, or `default` |
| `--clamping <mode>` | Set texture clamping: `clamp`, `border`, `repeat` |
| `--disable-mouse` | Disable mouse interaction |
| `--no-fullscreen-pause` | Prevent pausing while fullscreen apps are running |

## 🧪 Wayland & X11 Support

- **Wayland**: Works with compositors that support `wlr-layer-shell-unstable`.
- **X11**: Requires XRandr. Use `--screen-root <screen_name>` (as shown in `xrandr`).

> ⚠ For X11 users: Currently doesn't work if a compositor or desktop environment (e.g. GNOME, KDE, Nautilus) is drawing the background.

---

## 🌈 Example Backgrounds

![example1](docs/images/example.gif)
![example2](docs/images/example2.gif)

Want to see more examples of backgrounds that work? Head over to the [project's website](https://wpengine.alma.mu/#showcase)

## 🪲 Common issues
### Black screen when setting as screen's background
This can be caused by a few different things depending on your environment and setup.

### X11
Common symptom of a compositor drawing to the background which prevents Wallpaper Engine from being properly visible.
The only solution currently is disabling the compositor so Wallpaper Engine can properly draw on the screen

### NVIDIA
Some users have had issues with GLFW initialization and other OpenGL errors. These are generally something that's
worth reporting in the issues. Sometimes adding this variable when running Wallpaper Engine helps and/or solves
the issue:
```bash
__GL_THREADED_OPTIMIZATIONS=0 linux-wallpaperengine
```

We'll be looking at improving this in the future, but for now it can be a useful workaround.

---

## 🙏 Special Thanks

- [RePKG](https://github.com/notscuffed/repkg) – for texture flag insights
- [RenderDoc](https://github.com/baldurk/renderdoc) – the best OpenGL debugger out there!
