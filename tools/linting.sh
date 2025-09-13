#!/bin/bash

shopt -s globstar
clang-tidy --format-style file src/WallpaperEngine/**/*.cpp src/*.cpp src/Steam/**/*.cpp -p cmake-build-debug