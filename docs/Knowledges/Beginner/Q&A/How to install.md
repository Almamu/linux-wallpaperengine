# 🚀 How to Install: Linux Wallpaper Engine

## 📦 System Requirements

### To compile and run this, you'll need:

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

### To use this program, you'll need:

1. **Official Wallpaper Engine's wallpaper assets (via Steam)**
   1. Let see this: [How to install - Wallpaper Engine](How%20to%20install%20-%20Wallpape%20Engine.md)

### Install the required dependencies on Ubuntu/Debian-based systems:

#### Ubuntu 22.04
```bash
sudo apt-get update
sudo apt-get install build-essential cmake libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev libglew-dev freeglut3-dev libsdl2-dev liblz4-dev libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libxxf86vm-dev libglm-dev libglfw3-dev libmpv-dev mpv libmpv1 libpulse-dev libpulse0 libfftw3-dev
```

#### Ubuntu 24.04
```bash
sudo apt-get update
sudo apt-get install build-essential cmake libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev libglew-dev freeglut3-dev libsdl2-dev liblz4-dev libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libxxf86vm-dev libglm-dev libglfw3-dev libmpv-dev mpv libmpv2 libpulse-dev libpulse0 libfftw3-dev
```

---

## 🐧 Arch Linux Users

You can install this directly from the AUR using your favorite AUR helper:

```bash
yay -S linux-wallpaperengine-git
```

> This installs the latest development version.

### Build from Source

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

> ✅ Remember: Place the `assets` folder next to the built binary if it isn’t detected automatically.
