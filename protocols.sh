#!/bin/sh

$(pkg-config --variable=wayland_scanner wayland-scanner) client-header protocols/wlr-layer-shell-unstable-v1.xml wlr-layer-shell-unstable-v1-protocol.h
$(pkg-config --variable=wayland_scanner wayland-scanner) private-code protocols/wlr-layer-shell-unstable-v1.xml wlr-layer-shell-unstable-v1-protocol.c
$(pkg-config --variable=wayland_scanner wayland-scanner) client-header $(pkg-config --variable=pkgdatadir wayland-protocols)/stable/xdg-shell/xdg-shell.xml xdg-shell-protocol.h
$(pkg-config --variable=wayland_scanner wayland-scanner) private-code $(pkg-config --variable=pkgdatadir wayland-protocols)/stable/xdg-shell/xdg-shell.xml xdg-shell-protocol.c

cc -c -I. -std=c99 -lwayland-client -Wall -Wextra -Werror -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function -Wno-unused-variable -Wno-unused-result -Wdeclaration-after-statement wlr-layer-shell-unstable-v1-protocol.c -o wlr-layer-shell-unstable-v1-protocol.o
cc -c -I. -std=c99 -lwayland-client -Wall -Wextra -Werror -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function -Wno-unused-variable -Wno-unused-result -Wdeclaration-after-statement xdg-shell-protocol.c -o xdg-shell-protocol.o