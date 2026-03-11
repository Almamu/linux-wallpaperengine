# Development Guide

This document provides information on how to build and test the application for development purposes.

The target `linux-wallpaperengine-dev-viewer` is designed to simplify testing and validation of new features. It provides a fully contained, statically linked binary with everything in it, allowing for a faster development cycle.

## Building for Development

To build the project for development, it's recommended to use the `linux-wallpaperengine-dev-viewer` target.

### Prerequisites

Ensure you have all dependencies installed as listed in the [README.md](../README.md).

### Build steps

1. Create a build directory:
   ```bash
   mkdir build
   cd build
   ```

2. Configure the project with CMake:
   ```bash
   cmake .. -DCMAKE_BUILD_TYPE=Debug
   ```

3. Build the development viewer:
   ```bash
   cmake --build . --target linux-wallpaperengine-dev-viewer
   ```

The resulting binary will be located in `output/linux-wallpaperengine-dev-viewer` within your build directory.

## Using the Development Viewer

The `linux-wallpaperengine-dev-viewer` allows you to quickly load and test wallpapers without having to run the full application.

### Command-line Arguments

- `-a`, `--assets`: Path to WallpaperEngine's assets.
- `-s`, `--steam-dir`: Path to your Steam installation.
- `-p`, `--property`: Sets a property of the background (e.g., `key=value`).
- `wallpaper`: The wallpaper to display (path or ID).

### Examples

Assuming you are in the `output` directory of your build:

#### Load a wallpaper from a folder
```bash
./linux-wallpaperengine-dev-viewer /path/to/wallpaper/folder
```

#### Load a wallpaper by ID
```bash
./linux-wallpaperengine-dev-viewer 123456789
```

#### Load a wallpaper with specific properties
```bash
./linux-wallpaperengine-dev-viewer /path/to/wallpaper/folder -p some_property=true -p another_property=value
```

## Running Tests

If you want to run the tests, make sure to build the `tests` target:

```bash
cmake --build . --target tests
./output/tests
```
