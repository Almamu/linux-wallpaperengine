<p align="center">
	<a href="https://github.com/Almamu/linux-wallpaperengine/blob/main/LICENSE"><img src="https://img.shields.io/github/license/Almamu/linux-wallpaperengine" /></a>
</p>

# 1. Disclaimer
**This is an educational project**. The project started as a fork from [Almamu/linux-wallpaperengine](https://github.com/Almamu/linux-wallpaperengine) so please check it out!
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
sudo apt-get install build-essential cmake libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev libglew-dev freeglut3-dev libsdl2-dev liblz4-dev libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libxxf86vm-dev libglm-dev libglfw3-dev libmpv-dev mpv libmpv1 libpulse-dev libpulse0 qtbase6-dev

# Fedora
sudo dnf install @development-tools cmake libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel mesa-libGL-devel glew-devel freeglut-devel SDL2-devel lz4-devel libXxf86vm-devel glm-devel glfw-devel mpv-devel qt6-qtbase-devel
```

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
git clone --recurse-submodules https://github.com/Deliasama/linux-wallpaperengine.git
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

## 2.1. Running linux-wallpaperengine
Now you can run the program using the following command:

```
./linux-wallpaperengine
```

#### Wayland
Has only been tested under wlroots but should work on any flavour as long as wlr-layer-shell-unstable is supported.

#### X11
Only screens configured with the XRandr extension are supported.

**IMPORTANT: Right now this doesn't work if there is anything drawing to the background (like a compositor, gnome, kde, nautilus, etc)**

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
