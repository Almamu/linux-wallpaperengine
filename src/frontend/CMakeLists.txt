set(TARGET_NAME linux-wallpaperengine)
set(TARGET_SOURCE_FILES
    $<TARGET_OBJECTS:linux-wallpaperengine-common-object>
    WallpaperEngine/Application/ApplicationContext.cpp
    WallpaperEngine/Application/WallpaperApplication.cpp
    WallpaperEngine/Audio/Pulseaudio.cpp
    WallpaperEngine/Desktop/Universal/Environment.cpp
    WallpaperEngine/Desktop/Output.cpp

    main.cpp)
set(TARGET_LIBRARIES
    linux-wallpaperengine-common-object
    linux-wallpaperengine-glad
    linux-wallpaperengine-core
    PkgConfig::pulseaudio
    glfw
    argparse)

if(NOT WPBUILD_FRONTEND)
    return()
endif()

include(RequireArgParse)
find_package(linux-wallpaperengine-core REQUIRED)
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)
# detect wayland support
pkg_check_modules(WAYLAND_SUPPORT wayland-cursor wayland-protocols egl wayland-egl)
pkg_check_modules(pulseaudio REQUIRED IMPORTED_TARGET libpulse)

if(WAYLAND_SUPPORT_FOUND)
    find_program(WaylandScanner NAMES wayland-scanner)
    message(STATUS "Found WaylandScanner at ${WaylandScanner}")
    execute_process(
        COMMAND pkg-config --variable=pkgdatadir wayland-protocols
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE WAYLAND_PROTOCOLS_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    message(STATUS "Found wayland-protocols at ${WAYLAND_PROTOCOLS_DIR}")
    set(WAYLAND_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/wayland)

    file(MAKE_DIRECTORY ${WAYLAND_OUTPUT_DIR})

    message(STATUS "Building protocols...")
    add_custom_command(OUTPUT ${WAYLAND_OUTPUT_DIR}/wlr-foreign-toplevel-management-unstable-v1-protocol.h
        COMMAND ${WaylandScanner} client-header ${CMAKE_CURRENT_SOURCE_DIR}/protocols/wlr-foreign-toplevel-management-unstable-v1.xml ${WAYLAND_OUTPUT_DIR}/wlr-foreign-toplevel-management-unstable-v1-protocol.h)
    add_custom_command(OUTPUT ${WAYLAND_OUTPUT_DIR}/wlr-foreign-toplevel-management-unstable-v1-protocol.c
        COMMAND ${WaylandScanner} private-code ${CMAKE_CURRENT_SOURCE_DIR}/protocols/wlr-foreign-toplevel-management-unstable-v1.xml ${WAYLAND_OUTPUT_DIR}/wlr-foreign-toplevel-management-unstable-v1-protocol.c)
    add_custom_command(OUTPUT ${WAYLAND_OUTPUT_DIR}/wlr-layer-shell-unstable-v1-protocol.h
        COMMAND ${WaylandScanner} client-header ${CMAKE_CURRENT_SOURCE_DIR}/protocols/wlr-layer-shell-unstable-v1.xml ${WAYLAND_OUTPUT_DIR}/wlr-layer-shell-unstable-v1-protocol.h)
    add_custom_command(OUTPUT ${WAYLAND_OUTPUT_DIR}/wlr-layer-shell-unstable-v1-protocol.c
        COMMAND ${WaylandScanner} private-code ${CMAKE_CURRENT_SOURCE_DIR}/protocols/wlr-layer-shell-unstable-v1.xml ${WAYLAND_OUTPUT_DIR}/wlr-layer-shell-unstable-v1-protocol.c)
    add_custom_command(OUTPUT ${WAYLAND_OUTPUT_DIR}/xdg-shell-protocol.h
        COMMAND ${WaylandScanner} client-header ${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml ${WAYLAND_OUTPUT_DIR}/xdg-shell-protocol.h)
    add_custom_command(OUTPUT ${WAYLAND_OUTPUT_DIR}/xdg-shell-protocol.c
        COMMAND ${WaylandScanner} private-code ${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml ${WAYLAND_OUTPUT_DIR}/xdg-shell-protocol.c)

    # add wayland sources to the target's source files
    set(TARGET_SOURCE_FILES
        ${TARGET_SOURCE_FILES}

        ${WAYLAND_OUTPUT_DIR}/xdg-shell-protocol.c
        ${WAYLAND_OUTPUT_DIR}/xdg-shell-protocol.h
        ${WAYLAND_OUTPUT_DIR}/wlr-foreign-toplevel-management-unstable-v1-protocol.c
        ${WAYLAND_OUTPUT_DIR}/wlr-foreign-toplevel-management-unstable-v1-protocol.h
        ${WAYLAND_OUTPUT_DIR}/wlr-layer-shell-unstable-v1-protocol.c
        ${WAYLAND_OUTPUT_DIR}/wlr-layer-shell-unstable-v1-protocol.h

        WallpaperEngine/Desktop/Wayland/Environment.cpp
        WallpaperEngine/Desktop/Wayland/Output.cpp)
    set(TARGET_INCLUDE_DIRS
        ${WAYLAND_SUPPORT_INCLUDE_DIRS}
        ${WAYLAND_OUTPUT_DIR})
    set(TARGET_LIBRARIES
        ${TARGET_LIBRARIES}

        pthread
        wayland-cursor
        wayland-client
        wayland-egl
        ${OPENGL_egl_LIBRARY})
endif()

add_executable(${TARGET_NAME} ${TARGET_SOURCE_FILES})

target_include_directories(${TARGET_NAME} PUBLIC
    ${TARGET_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${EXTERNAL_DEPS_DIR}/stb)

target_link_libraries (${TARGET_NAME} PUBLIC ${TARGET_LIBRARIES})
