#!/bin/bash
shopt -s globstar
clang-format -i src/common/**/*.{cpp,h} src/core/**/*.{cpp,h} src/dev-viewer/**/*.{cpp,h} src/frontend/**/*.{cpp,h} src/glad/**/*.{c,h} src/webhelper/**/*.{cpp,h} include/frontends/*.h