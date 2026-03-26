#!/bin/bash
shopt -s globstar
clang-format -i src/WallpaperEngine/**/*.{cpp,h} src/Steam/**/*.{cpp,h} src/frontends/**/*.cpp include/frontends/*.h