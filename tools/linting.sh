#!/bin/bash

shopt -s globstar
clang-tidy --format-style file src/**/*.cpp -p cmake-build-debug