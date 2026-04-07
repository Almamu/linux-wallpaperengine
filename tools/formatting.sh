#!/bin/bash
shopt -s globstar
clang-format -i src/core/**/*.{cpp,h} src/glad/**/*.{c,cpp,h} src/dev-viewer/**/*.{cpp,h} src/frontend/**/*.{cpp,h} src/webhelper/**/*.{cpp,h} include/frontends/*.h