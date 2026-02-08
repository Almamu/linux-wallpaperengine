#!/bin/bash
shopt -s globstar
clang-format -i src/WallpaperEngine/**/*.{cpp,h} src/*.{cpp,h} src/Steam/**/*.{cpp,h}