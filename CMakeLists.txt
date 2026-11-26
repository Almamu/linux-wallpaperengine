cmake_minimum_required(VERSION 3.15)
project(linux-wallpaperengine)

# prevent installing to /usr by default so we don't bork anything
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/opt/${PROJECT_NAME}" CACHE PATH "..." FORCE)
endif()

set_property(GLOBAL PROPERTY OS_FOLDERS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")
set(OpenGL_GL_PREFERENCE "LEGACY")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# puts the app in demomode, which records a 5 second webm video and quits
if(NOT DEMOMODE)
    set(DEMOMODE 0)
endif()

# if you're developing you might find this debug option useful for shader output, although RenderDoc is encouraged
if(NOT ERRORONLY)
    set(ERRORONLY 0)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-narrowing")

find_package(X11)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

find_package(DBus REQUIRED)
find_package(GLUT REQUIRED)
find_package(ZLIB REQUIRED)
find_package(SDL2 REQUIRED)
find_package(MPV REQUIRED)
find_package(LZ4 REQUIRED)
find_package(FFMPEG REQUIRED)
find_package(PulseAudio REQUIRED)
find_package(Freetype REQUIRED)
find_package(Fontconfig REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(HARFBUZZ REQUIRED IMPORTED_TARGET harfbuzz)
include(CMakeModules/PrivateMSDF.cmake)

set(ENABLE_OPT OFF)
set(SPIRV_CROSS_FORCE_PIC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(KISSFFT_TEST OFF)
set(KISSFFT_TOOLS OFF)

# Download CEF of specified version for current platform
# Specify the CEF distribution version.
set(CEF_VERSION "150.0.9+g81b0088+chromium-150.0.7871.46")
# CEF_DISTRIBUTION_TYPE default value should be different based on the type of build
# debug builds should use standard versions
# while release version can do with the minimal version
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(default_cef_dist "standard")
else()
    set(default_cef_dist "minimal")
endif()

set(CEF_DISTRIBUTION_TYPE "${default_cef_dist}" CACHE STRING "CEF distribution variant")
set_property(CACHE CEF_DISTRIBUTION_TYPE PROPERTY STRINGS "minimal" "standard")
# Determine the platform.
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
    if("${PROJECT_ARCH}" STREQUAL "arm64")
        set(CEF_PLATFORM "macosarm64")
        set(CEF_ARCH_DETECTION "${PROJECT_ARCH}")
    elseif("${PROJECT_ARCH}" STREQUAL "x86_64")
        set(CEF_PLATFORM "macosx64")
        set(CEF_ARCH_DETECTION "${PROJECT_ARCH}")
    elseif("${CMAKE_HOST_SYSTEM_PROCESSOR}" STREQUAL "arm64")
        set(PROJECT_ARCH "arm64")
        set(CEF_PLATFORM "macosarm64")
        set(CEF_ARCH_DETECTION "${CMAKE_HOST_SYSTEM_PROCESSOR}")
    else()
        set(PROJECT_ARCH "x86_64")
        set(CEF_PLATFORM "macosx64")
        set(CEF_ARCH_DETECTION "Default for Darwin")
    endif()
elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "arm")
        set(CEF_PLATFORM "linuxarm")
        set(CEF_ARCH_DETECTION "${CMAKE_SYSTEM_PROCESSOR}")
    elseif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "arm64")
        set(PROJECT_ARCH "arm64")
        set(CEF_PLATFORM "linuxarm64")
        set(CEF_ARCH_DETECTION "${CMAKE_SYSTEM_PROCESSOR}")
    elseif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "aarch64")
        set(PROJECT_ARCH "arm64")
        set(CEF_PLATFORM "linuxarm64")
        set(CEF_ARCH_DETECTION "${CMAKE_SYSTEM_PROCESSOR}")
    elseif(CMAKE_SIZEOF_VOID_P MATCHES 8)
        set(CEF_PLATFORM "linux64")
        set(CEF_ARCH_DETECTION "Based on void size ${CMAKE_SIZEOF_VOID_P} (${CMAKE_SYSTEM_PROCESSOR})")
    else()
        message(FATAL_ERROR "Linux x86 32-bit builds are discontinued.")
    endif()
elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
    if("${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}" STREQUAL "ARM64")
        set(CEF_PLATFORM "windowsarm64")
        set(CEF_ARCH_DETECTION "${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}")
    elseif(CMAKE_SIZEOF_VOID_P MATCHES 8)
        set(CEF_PLATFORM "windows64")
        set(CEF_ARCH_DETECTION "Based on void size ${CMAKE_SIZEOF_VOID_P}")
    else()
        set(CEF_PLATFORM "windows32")
        set(CEF_ARCH_DETECTION "Default for Windows")
    endif()
endif()

if(CEF_DISTRIBUTION_TYPE AND NOT CEF_DISTRIBUTION_TYPE STREQUAL "standard")
    set(CEF_PLATFORM "${CEF_PLATFORM}_${CEF_DISTRIBUTION_TYPE}")
endif()
message(STATUS "Using CEF for ${CMAKE_SYSTEM_NAME} - ${CEF_PLATFORM} (${CEF_ARCH_DETECTION})")
include(DownloadCEF)
DownloadCEF("${CEF_PLATFORM}" "${CEF_VERSION}" "${CMAKE_CURRENT_BINARY_DIR}/cef")

# add cef's cmake files to the lookup so we can use those directly
list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")

find_package(CEF REQUIRED)

# remove some switches that forbid us from using things that are commonplace in our codebase
list(REMOVE_ITEM CEF_CXX_COMPILER_FLAGS -fno-exceptions)
list(REMOVE_ITEM CEF_CXX_COMPILER_FLAGS -fno-rtti)
list(REMOVE_ITEM CEF_CXX_COMPILER_FLAGS -std=c++17)
list(REMOVE_ITEM CEF_COMPILER_FLAGS -fvisibility=hidden)
list(REMOVE_ITEM CEF_COMPILER_FLAGS -fvisibility-inlines-hidden)
list(APPEND CEF_CXX_COMPILER_FLAGS -std=c++20)
list(APPEND CEF_EXE_LINKER_FLAGS -rdynamic)
# remove the vulkan library from the files to copy so it doesn't fail after removing it
list(REMOVE_ITEM CEF_BINARY_FILES libvulkan.so.1)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/output)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(TARGET_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/output)

# RPATH configuration: use build directories during development, but $ORIGIN after installation
# NOTE: \$ORIGIN must be escaped because CMake treats $VAR as variable expansion.
# IMPORTANT: This must be set BEFORE any add_executable()/add_library() calls in this directory.
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH "\$ORIGIN;\$ORIGIN/lib;\$ORIGIN/lib64")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)

add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)
add_subdirectory(src/External/glslang-WallpaperEngine glslang)
add_subdirectory(src/External/SPIRV-Cross-WallpaperEngine spirv-cross)
add_subdirectory(src/External/kissfft kissfft)
include(CMakeModules/PrivateOpenAL.cmake)
add_subdirectory(src/External/argparse argparse)
set(QJS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
option(WPENGINE_SCENESCRIPT_JIT "Use JavaScriptCore for JIT-compiled SceneScript" ON)
if(WPENGINE_SCENESCRIPT_JIT)
    pkg_check_modules(JAVASCRIPTCORE QUIET IMPORTED_TARGET javascriptcoregtk-4.1)
endif()
if(WPENGINE_SCENESCRIPT_JIT AND JAVASCRIPTCORE_FOUND)
    add_library(scenescript-runtime STATIC src/WallpaperEngine/Scripting/JavaScriptCoreRuntime.cpp)
    set_target_properties(scenescript-runtime PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_include_directories(scenescript-runtime PUBLIC src/External/quickjs)
    target_link_libraries(scenescript-runtime PUBLIC PkgConfig::JAVASCRIPTCORE)
    target_compile_definitions(scenescript-runtime PUBLIC WPENGINE_JAVASCRIPTCORE=1)
    set(WPENGINE_SCRIPT_RUNTIME scenescript-runtime)
    message(STATUS "SceneScript runtime: JavaScriptCore (JIT)")
else()
    add_subdirectory(src/External/quickjs quickjs)
    set(WPENGINE_SCRIPT_RUNTIME qjs)
    message(STATUS "SceneScript runtime: QuickJS (interpreter)")
endif()
if(BUILD_TESTING)
    add_subdirectory(src/External/Catch2)
endif()
# try to enable wayland builds when possible
pkg_check_modules(WAYLAND_SUPPORT wayland-cursor wayland-protocols egl wayland-egl)

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
    add_custom_command(OUTPUT ${WAYLAND_OUTPUT_DIR}/xdg-output-unstable-v1-protocol.h
        COMMAND ${WaylandScanner} client-header ${WAYLAND_PROTOCOLS_DIR}/unstable/xdg-output/xdg-output-unstable-v1.xml ${WAYLAND_OUTPUT_DIR}/xdg-output-unstable-v1-protocol.h)
    add_custom_command(OUTPUT ${WAYLAND_OUTPUT_DIR}/xdg-output-unstable-v1-protocol.c
        COMMAND ${WaylandScanner} private-code ${WAYLAND_PROTOCOLS_DIR}/unstable/xdg-output/xdg-output-unstable-v1.xml ${WAYLAND_OUTPUT_DIR}/xdg-output-unstable-v1-protocol.c)

    add_custom_command(OUTPUT ${WAYLAND_OUTPUT_DIR}/color-management-v1-protocol.h
        COMMAND ${WaylandScanner} client-header ${CMAKE_CURRENT_SOURCE_DIR}/protocols/color-management-v1.xml ${WAYLAND_OUTPUT_DIR}/color-management-v1-protocol.h
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/protocols/color-management-v1.xml)
    add_custom_command(OUTPUT ${WAYLAND_OUTPUT_DIR}/color-management-v1-protocol.c
        COMMAND ${WaylandScanner} private-code ${CMAKE_CURRENT_SOURCE_DIR}/protocols/color-management-v1.xml ${WAYLAND_OUTPUT_DIR}/color-management-v1-protocol.c
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/protocols/color-management-v1.xml)

    include_directories(${WAYLAND_SUPPORT_INCLUDE_DIRS})
    include_directories(${WAYLAND_OUTPUT_DIR})
    set(WAYLAND_LIBRARIES
        pthread
        wayland-cursor
        wayland-client
        wayland-egl
        ${OPENGL_egl_LIBRARY})
    set(WAYLAND_SOURCES
        src/WallpaperEngine/Render/Drivers/WaylandColorManagement.h
        src/WallpaperEngine/Render/Drivers/WaylandColorManagement.cpp
        src/WallpaperEngine/Render/Drivers/WaylandEGLConfig.h
        src/WallpaperEngine/Render/Drivers/WaylandEGLConfig.cpp
        ${WAYLAND_OUTPUT_DIR}/color-management-v1-protocol.h
        ${WAYLAND_OUTPUT_DIR}/color-management-v1-protocol.c
        src/WallpaperEngine/Render/Drivers/WaylandOpenGLDriver.h
        src/WallpaperEngine/Render/Drivers/WaylandOpenGLDriver.cpp
        src/WallpaperEngine/Render/Drivers/Detectors/WaylandFullScreenDetector.cpp
        src/WallpaperEngine/Render/Drivers/Detectors/WaylandFullScreenDetector.h
        src/WallpaperEngine/Render/Drivers/Output/WaylandOutput.cpp
        src/WallpaperEngine/Render/Drivers/Output/WaylandOutput.h
        src/WallpaperEngine/Render/Drivers/Output/WaylandOutputViewport.cpp
        src/WallpaperEngine/Render/Drivers/Output/WaylandOutputViewport.h
        src/WallpaperEngine/Input/Drivers/WaylandMouseInput.cpp
        src/WallpaperEngine/Input/Drivers/WaylandMouseInput.h
        ${WAYLAND_OUTPUT_DIR}/xdg-shell-protocol.c
        ${WAYLAND_OUTPUT_DIR}/xdg-shell-protocol.h
        ${WAYLAND_OUTPUT_DIR}/wlr-foreign-toplevel-management-unstable-v1-protocol.c
        ${WAYLAND_OUTPUT_DIR}/wlr-foreign-toplevel-management-unstable-v1-protocol.h
        ${WAYLAND_OUTPUT_DIR}/wlr-layer-shell-unstable-v1-protocol.c
        ${WAYLAND_OUTPUT_DIR}/wlr-layer-shell-unstable-v1-protocol.h
        ${WAYLAND_OUTPUT_DIR}/xdg-output-unstable-v1-protocol.c
        ${WAYLAND_OUTPUT_DIR}/xdg-output-unstable-v1-protocol.h)
endif()

if(X11_FOUND)
    if(NOT X11_Xrandr_FOUND)
        message(WARNING "X11 support disabled. Xrandr package is missing")
        set(X11_SUPPORT_FOUND FALSE)
    elseif(NOT X11_xf86vmode_FOUND)
        message(WARNING "X11 support disabled. Xxf86vm package is missing")
        set(X11_SUPPORT_FOUND FALSE)
    else()
        message("X11 support enabled")
        set(X11_SUPPORT_FOUND TRUE)
    endif()
endif()

if(X11_SUPPORT_FOUND)
    include_directories(${X11_INCLUDE_DIR} ${XRANDR_INCLUDE_DIR})

    set(X11_LIBRARIES
        ${X11_LIBRARIES}
        ${X11_Xrandr_LIB})
    set(X11_SOURCES
        src/WallpaperEngine/Render/Drivers/Output/X11Output.cpp
        src/WallpaperEngine/Render/Drivers/Output/X11Output.h
        src/WallpaperEngine/Render/Drivers/Detectors/X11FullScreenDetector.cpp
        src/WallpaperEngine/Render/Drivers/Detectors/X11FullScreenDetector.h)
    SET(X11_INCLUDES
        ${X11_INCLUDE_DIR}
        ${XRANDR_INCLUDE_DIR})
    set(CMAKE_REQUIRED_LIBRARIES ${X11_LIBRARIES})
endif()

if(DEMOMODE)
    set(DEMOMODE_SOURCES
        src/recording.cpp
        src/recording.h)
    message(WARNING "Enabling demo mode will automatically record 5 seconds and stop the software. This is used internally to produce the video seen on the website as a sort of status report")
endif()
if(NOT WAYLAND_SUPPORT_FOUND AND NOT X11_SUPPORT_FOUND)
    message(WARNING "No window server detected at build time. You will only be able to preview backgrounds")
endif()

add_library(ceflib SHARED IMPORTED)
set_target_properties(ceflib
        PROPERTIES IMPORTED_LOCATION ${TARGET_OUTPUT_DIRECTORY}/libcef.so)

ADD_LOGICAL_TARGET(libcef_lib "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")
PRINT_CEF_CONFIG()

# Embed scene-script builtins (builtins.js) into a generated header, so the JS can
# live in its own file (lint/syntax-highlight friendly) instead of an inline literal.
# Added to the global include_directories so all engine targets can find the
# generated header.
set(WPENGINE_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(WPENGINE_BUILTINS_JS "${CMAKE_CURRENT_SOURCE_DIR}/src/WallpaperEngine/Scripting/resources/builtins.js")
file(READ "${WPENGINE_BUILTINS_JS}" WPENGINE_SCRIPT_BUILTINS)
set(WPENGINE_VECTORS_JS "${CMAKE_CURRENT_SOURCE_DIR}/src/WallpaperEngine/Scripting/resources/vectors.js")
file(READ "${WPENGINE_VECTORS_JS}" WPENGINE_SCRIPT_VECTORS)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/WallpaperEngine/Scripting/resources/Builtins.generated.h.in"
    "${WPENGINE_GENERATED_DIR}/WallpaperEngine/Scripting/Builtins.generated.h"
    @ONLY)
# re-run configuration (regenerating the header) whenever the .js changes
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${WPENGINE_BUILTINS_JS}" "${WPENGINE_VECTORS_JS}")

# Invalidate persistent shader results automatically when processing code, bundled
# translators or build options change, including uncommitted source edits.
file(GLOB_RECURSE WPENGINE_SHADER_CACHE_SOURCES CONFIGURE_DEPENDS
    "src/WallpaperEngine/Render/Shaders/*.cpp" "src/WallpaperEngine/Render/Shaders/*.h"
    "src/External/glslang-WallpaperEngine/*.cpp" "src/External/glslang-WallpaperEngine/*.h"
    "src/External/glslang-WallpaperEngine/*.hpp" "src/External/glslang-WallpaperEngine/*.inc"
    "src/External/glslang-WallpaperEngine/*.in" "src/External/glslang-WallpaperEngine/*.cmake"
    "src/External/glslang-WallpaperEngine/CMakeLists.txt"
    "src/External/SPIRV-Cross-WallpaperEngine/*.cpp" "src/External/SPIRV-Cross-WallpaperEngine/*.hpp"
    "src/External/SPIRV-Cross-WallpaperEngine/*.h" "src/External/SPIRV-Cross-WallpaperEngine/*.inc"
    "src/External/SPIRV-Cross-WallpaperEngine/*.in" "src/External/SPIRV-Cross-WallpaperEngine/*.cmake"
    "src/External/SPIRV-Cross-WallpaperEngine/CMakeLists.txt")
list(APPEND WPENGINE_SHADER_CACHE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt")
list(SORT WPENGINE_SHADER_CACHE_SOURCES)
set(WPENGINE_SHADER_CACHE_ID "v1;${CMAKE_CXX_COMPILER_ID};${CMAKE_CXX_COMPILER_VERSION};${CMAKE_SYSTEM_NAME};${CMAKE_SYSTEM_PROCESSOR};${CMAKE_BUILD_TYPE}")
get_cmake_property(WPENGINE_SHADER_CACHE_OPTIONS CACHE_VARIABLES)
list(SORT WPENGINE_SHADER_CACHE_OPTIONS)
foreach(WPENGINE_SHADER_CACHE_OPTION IN LISTS WPENGINE_SHADER_CACHE_OPTIONS)
    if(WPENGINE_SHADER_CACHE_OPTION MATCHES "^(CMAKE_CXX_FLAGS|ENABLE_|SPIRV_|GLSLANG_)")
        string(APPEND WPENGINE_SHADER_CACHE_ID "${WPENGINE_SHADER_CACHE_OPTION}=${${WPENGINE_SHADER_CACHE_OPTION}};")
    endif()
endforeach()
foreach(WPENGINE_SHADER_CACHE_SOURCE IN LISTS WPENGINE_SHADER_CACHE_SOURCES)
    file(SHA256 "${WPENGINE_SHADER_CACHE_SOURCE}" WPENGINE_SHADER_CACHE_HASH)
    string(APPEND WPENGINE_SHADER_CACHE_ID "${WPENGINE_SHADER_CACHE_HASH};")
endforeach()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${WPENGINE_SHADER_CACHE_SOURCES})
string(SHA256 WPENGINE_SHADER_CACHE_VERSION "${WPENGINE_SHADER_CACHE_ID}")
configure_file(
    src/WallpaperEngine/Render/Shaders/ShaderCacheVersion.h.in
    "${WPENGINE_GENERATED_DIR}/ShaderCacheVersion.generated.h" @ONLY)

include_directories(
    ${WPENGINE_GENERATED_DIR}
    src
    src/External/glslang-WallpaperEngine
    src/External/SPIRV-Cross-WallpaperEngine
    src/External/MimeTypes
    src/External/json/include
    src/External/stb
    src/External/kissfft
    ${MPV_INCLUDE_DIR}
    ${GLEW_INCLUDE_DIR}
    ${LZ4_INCLUDE_DIR}
    ${SDL2_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIR}
    ${PULSEAUDIO_INCLUDE_DIR}
    ${CEF_INCLUDE_PATH}
    ${CMAKE_SOURCE_DIR}
    ${X11_INCLUDES}
    ${DBUS_INCLUDE_DIRS})

set(COMMON_SOURCES
    src/External/MimeTypes/MimeTypes.cpp
    src/External/MimeTypes/MimeTypes.h

    src/Steam/FileSystem/FileSystem.cpp
    src/Steam/FileSystem/FileSystem.h

    src/WallpaperEngine/Logging/Log.cpp
    src/WallpaperEngine/Logging/Log.h

    src/WallpaperEngine/Debugging/CallStack.cpp
    src/WallpaperEngine/Debugging/CallStack.h

    src/WallpaperEngine/Data/Model/CameraPath.h
    src/WallpaperEngine/Data/Model/CameraPath.cpp
    src/WallpaperEngine/Data/Parsers/CameraPathParser.h
    src/WallpaperEngine/Data/Parsers/CameraPathParser.cpp

    src/WallpaperEngine/Application/ApplicationContext.cpp
    src/WallpaperEngine/Application/ApplicationContext.h
    src/WallpaperEngine/Application/PlaylistState.cpp
    src/WallpaperEngine/Application/PlaylistState.h
    src/WallpaperEngine/Application/WallpaperApplication.cpp
    src/WallpaperEngine/Application/WallpaperApplication.h
    src/WallpaperEngine/Application/LockscreenStream.h

    src/WallpaperEngine/Assets/AssetLoadException.cpp
    src/WallpaperEngine/Assets/AssetLoadException.h
    src/WallpaperEngine/Assets/AssetLocator.cpp
    src/WallpaperEngine/Assets/AssetLocator.h

    src/WallpaperEngine/FileSystem/Container.h
    src/WallpaperEngine/FileSystem/Container.cpp
    src/WallpaperEngine/FileSystem/Adapters/Types.h
    src/WallpaperEngine/FileSystem/Adapters/Directory.cpp
    src/WallpaperEngine/FileSystem/Adapters/Directory.h
    src/WallpaperEngine/FileSystem/Adapters/Package.cpp
    src/WallpaperEngine/FileSystem/Adapters/Package.h
    src/WallpaperEngine/FileSystem/Adapters/Virtual.cpp
    src/WallpaperEngine/FileSystem/Adapters/Virtual.h
    src/WallpaperEngine/FileSystem/Adapters/MediaCover.h
    src/WallpaperEngine/FileSystem/Adapters/MediaCover.cpp

    src/WallpaperEngine/Audio/Drivers/Recorders/PulseAudioPlaybackRecorder.cpp
    src/WallpaperEngine/Audio/Drivers/Recorders/PulseAudioPlaybackRecorder.h
    src/WallpaperEngine/Audio/Drivers/Recorders/PlaybackRecorder.cpp
    src/WallpaperEngine/Audio/Drivers/Recorders/PlaybackRecorder.h
    src/WallpaperEngine/Audio/Drivers/Recorders/WallpaperEngineSpectrumAnalyzer.cpp
    src/WallpaperEngine/Audio/Drivers/Recorders/WallpaperEngineSpectrumAnalyzer.h

    src/WallpaperEngine/Audio/Drivers/Detectors/PulseAudioPlayingDetector.cpp
    src/WallpaperEngine/Audio/Drivers/Detectors/PulseAudioPlayingDetector.h
    src/WallpaperEngine/Audio/Drivers/Detectors/AudioPlayingDetector.cpp
    src/WallpaperEngine/Audio/Drivers/Detectors/AudioPlayingDetector.h

    src/WallpaperEngine/Audio/Drivers/AudioDriver.cpp
    src/WallpaperEngine/Audio/Drivers/AudioDriver.h
    src/WallpaperEngine/Audio/Drivers/SDLAudioDriver.cpp
    src/WallpaperEngine/Audio/Drivers/SDLAudioDriver.h
    src/WallpaperEngine/Audio/AudioContext.cpp
    src/WallpaperEngine/Audio/AudioContext.h
    src/WallpaperEngine/Audio/SoundSpatialMixer.cpp
    src/WallpaperEngine/Audio/SoundOutputMetadata.cpp
    src/WallpaperEngine/Audio/AudioStream.cpp
    src/WallpaperEngine/Audio/AudioStream.h

    src/WallpaperEngine/Input/InputContext.cpp
    src/WallpaperEngine/Input/InputContext.h
    src/WallpaperEngine/Input/MouseInput.h
    src/WallpaperEngine/Input/Drivers/GLFWMouseInput.cpp
    src/WallpaperEngine/Input/Drivers/GLFWMouseInput.h

    src/WallpaperEngine/Media/MediaSource.h
    src/WallpaperEngine/Media/MediaSource.cpp
    src/WallpaperEngine/Media/DBusMediaSource.h
    src/WallpaperEngine/Media/DBusMediaSource.cpp

    src/WallpaperEngine/Render/Shaders/Variables/ShaderVariable.h
    src/WallpaperEngine/Render/Shaders/Variables/ShaderVariable.cpp
    src/WallpaperEngine/Render/Shaders/Variables/ShaderVariableFloat.h
    src/WallpaperEngine/Render/Shaders/Variables/ShaderVariableFloat.cpp
    src/WallpaperEngine/Render/Shaders/Variables/ShaderVariableInteger.h
    src/WallpaperEngine/Render/Shaders/Variables/ShaderVariableInteger.cpp
    src/WallpaperEngine/Render/Shaders/Variables/ShaderVariableVector2.h
    src/WallpaperEngine/Render/Shaders/Variables/ShaderVariableVector2.cpp
    src/WallpaperEngine/Render/Shaders/Variables/ShaderVariableVector3.h
    src/WallpaperEngine/Render/Shaders/Variables/ShaderVariableVector3.cpp
    src/WallpaperEngine/Render/Shaders/Variables/ShaderVariableVector4.h
    src/WallpaperEngine/Render/Shaders/Variables/ShaderVariableVector4.cpp

    src/WallpaperEngine/Render/Shaders/Shader.h
    src/WallpaperEngine/Render/Shaders/Shader.cpp
    src/WallpaperEngine/Render/Shaders/ShaderProgramCache.cpp
    src/WallpaperEngine/Render/Shaders/ShaderProgramCache.h
    src/WallpaperEngine/Render/Shaders/ShaderDiskCache.cpp
    src/WallpaperEngine/Render/Shaders/ShaderDiskCache.h
    src/WallpaperEngine/Render/Shaders/ShaderUnit.cpp
    src/WallpaperEngine/Render/Shaders/ShaderUnit.h
    src/WallpaperEngine/Render/Shaders/GLSLContext.cpp
    src/WallpaperEngine/Render/Shaders/GLSLContext.h

    src/WallpaperEngine/Debug/DebugHelpers.cpp
    src/WallpaperEngine/Debug/DebugHelpers.h
    src/WallpaperEngine/Debug/RenderHealth.cpp
    src/WallpaperEngine/Debug/RenderHealth.h

    src/WallpaperEngine/Render/Helpers/ContextAware.cpp
    src/WallpaperEngine/Render/Helpers/ContextAware.h

    src/WallpaperEngine/Render/Drivers/VideoFactories.cpp
    src/WallpaperEngine/Render/Drivers/VideoFactories.h

    src/WallpaperEngine/Render/Drivers/Detectors/FullScreenDetector.cpp
    src/WallpaperEngine/Render/Drivers/Detectors/FullScreenDetector.h

    src/WallpaperEngine/Render/Drivers/Output/Output.cpp
    src/WallpaperEngine/Render/Drivers/Output/Output.h
    src/WallpaperEngine/Render/Drivers/Output/GLFWWindowOutput.cpp
    src/WallpaperEngine/Render/Drivers/Output/GLFWWindowOutput.h
    src/WallpaperEngine/Render/Drivers/Output/GLFWOutputViewport.cpp
    src/WallpaperEngine/Render/Drivers/Output/GLFWOutputViewport.h
    src/WallpaperEngine/Render/Drivers/Output/OutputViewport.cpp
    src/WallpaperEngine/Render/Drivers/Output/OutputViewport.h
    src/WallpaperEngine/Render/Drivers/GLFWOpenGLDriver.h
    src/WallpaperEngine/Render/Drivers/GLFWOpenGLDriver.cpp
    src/WallpaperEngine/Render/Drivers/VideoDriver.h
    src/WallpaperEngine/Render/Drivers/VideoDriver.cpp
    src/WallpaperEngine/Render/RenderContext.h
    src/WallpaperEngine/Render/RenderContext.cpp
    src/WallpaperEngine/Render/TextureCache.h
    src/WallpaperEngine/Render/TextureCache.cpp
    src/WallpaperEngine/Render/FBOProvider.cpp
    src/WallpaperEngine/Render/FBOProvider.h

    src/WallpaperEngine/Render/Helpers/ContextAware.cpp
    src/WallpaperEngine/Render/Helpers/ContextAware.h

    src/WallpaperEngine/Render/CWallpaper.h
    src/WallpaperEngine/Render/TransitionMode.h
    src/WallpaperEngine/Render/CWallpaper.cpp
    src/WallpaperEngine/Render/WallpaperState.h
    src/WallpaperEngine/Render/WallpaperState.cpp
    src/WallpaperEngine/Render/Wallpapers/CScene.h
    src/WallpaperEngine/Render/Wallpapers/CScene.cpp
    src/WallpaperEngine/Render/Wallpapers/HdrBloom.cpp
    src/WallpaperEngine/Render/Wallpapers/VolumetricLights.cpp
    src/WallpaperEngine/Render/Wallpapers/CVideo.h
    src/WallpaperEngine/Render/Wallpapers/CVideo.cpp
    src/WallpaperEngine/Render/Wallpapers/CWeb.h
    src/WallpaperEngine/Render/Wallpapers/CWeb.cpp
    src/WallpaperEngine/Render/Camera.h
    src/WallpaperEngine/Render/Camera.cpp
    src/WallpaperEngine/Render/CObject.h
    src/WallpaperEngine/Render/CObject.cpp
    src/WallpaperEngine/Render/CTexture.cpp
    src/WallpaperEngine/Render/CTexture.h
    src/WallpaperEngine/Render/AlbumTexture.h
    src/WallpaperEngine/Render/AlbumTexture.cpp
    src/WallpaperEngine/Render/TextureProvider.h

    src/WallpaperEngine/Render/Objects/CRenderable.cpp
    src/WallpaperEngine/Render/Objects/CRenderable.h
    src/WallpaperEngine/Render/Objects/CImage.h
    src/WallpaperEngine/Render/Objects/CImage.cpp
    src/WallpaperEngine/Render/Objects/PuppetClippingPlan.h
    src/WallpaperEngine/Render/Objects/PuppetClippingPlan.cpp
    src/WallpaperEngine/Render/Objects/CSound.h
    src/WallpaperEngine/Render/Objects/CSound.cpp
    src/WallpaperEngine/Render/Objects/CParticle.h
    src/WallpaperEngine/Render/Objects/CParticle.cpp
    src/WallpaperEngine/Render/Objects/CText.h
    src/WallpaperEngine/Render/Objects/CText.cpp
    src/WallpaperEngine/Render/Objects/TextStyle.cpp
    src/WallpaperEngine/Render/Objects/TextMsdf.cpp
    src/WallpaperEngine/Render/Objects/CShape.h
    src/WallpaperEngine/Render/Objects/CShape.cpp
    src/WallpaperEngine/Render/Objects/CModel.h
    src/WallpaperEngine/Render/Objects/CModel.cpp
    src/WallpaperEngine/Render/Objects/CLight.h
    src/WallpaperEngine/Render/Objects/CLight.cpp

    src/WallpaperEngine/Render/CFBO.h
    src/WallpaperEngine/Render/CFBO.cpp
    src/WallpaperEngine/Render/Objects/Effects/CPass.h
    src/WallpaperEngine/Render/Objects/Effects/CPass.cpp

    src/WallpaperEngine/WebBrowser/CEF/RenderHandler.cpp
    src/WallpaperEngine/WebBrowser/CEF/RenderHandler.h
    src/WallpaperEngine/WebBrowser/CEF/BrowserClient.cpp
    src/WallpaperEngine/WebBrowser/CEF/BrowserClient.h
    src/WallpaperEngine/WebBrowser/CEF/BrowserApp.cpp
    src/WallpaperEngine/WebBrowser/CEF/BrowserApp.h
    src/WallpaperEngine/WebBrowser/CEF/SubprocessApp.cpp
    src/WallpaperEngine/WebBrowser/CEF/SubprocessApp.h
    src/WallpaperEngine/WebBrowser/CEF/WPSchemeHandlerFactory.cpp
    src/WallpaperEngine/WebBrowser/CEF/WPSchemeHandlerFactory.h
    src/WallpaperEngine/WebBrowser/CEF/WPSchemeHandler.cpp
    src/WallpaperEngine/WebBrowser/CEF/WPSchemeHandler.h
    src/WallpaperEngine/WebBrowser/WebBrowserContext.cpp
    src/WallpaperEngine/WebBrowser/WebBrowserContext.h

    src/WallpaperEngine/Data/Assets/Types.h
    src/WallpaperEngine/Data/Assets/Texture.h
    src/WallpaperEngine/Data/Assets/Package.h
    src/WallpaperEngine/Data/Model/Types.h
    src/WallpaperEngine/Data/Model/Project.h
    src/WallpaperEngine/Data/Model/Wallpaper.h
    src/WallpaperEngine/Data/Model/Object.h
    src/WallpaperEngine/Data/Model/Material.h
    src/WallpaperEngine/Data/Model/Effect.h
    src/WallpaperEngine/Data/Model/Model.h
    src/WallpaperEngine/Data/Model/MdlMesh.h
    src/WallpaperEngine/Data/Model/MdlAnimation.cpp
    src/WallpaperEngine/Data/Model/MdlAnimation.h
    src/WallpaperEngine/Data/Model/UserSetting.h
    src/WallpaperEngine/Data/Model/PropertyAnimation.h
    src/WallpaperEngine/Data/Model/PropertyAnimation.cpp
    src/WallpaperEngine/Data/Model/DynamicValue.h
    src/WallpaperEngine/Data/Model/DynamicValue.cpp
    src/WallpaperEngine/Data/Model/Property.h
    src/WallpaperEngine/Data/Utils/TypeCaster.h
    src/WallpaperEngine/Data/Utils/BinaryReader.cpp
    src/WallpaperEngine/Data/Utils/BinaryReader.h
    src/WallpaperEngine/Data/Utils/MemoryStream.h
    src/WallpaperEngine/Data/Utils/SFINAE.h
    src/WallpaperEngine/Data/Parsers/DynamicValueParser.h
    src/WallpaperEngine/Data/Parsers/DynamicValueParser.cpp
    src/WallpaperEngine/Data/Parsers/EffectParser.cpp
    src/WallpaperEngine/Data/Parsers/EffectParser.h
    src/WallpaperEngine/Data/Parsers/ProjectParser.cpp
    src/WallpaperEngine/Data/Parsers/ProjectParser.h
    src/WallpaperEngine/Data/Parsers/WallpaperParser.cpp
    src/WallpaperEngine/Data/Parsers/WallpaperParser.h
    src/WallpaperEngine/Data/Parsers/UserSettingParser.cpp
    src/WallpaperEngine/Data/Parsers/UserSettingParser.h
    src/WallpaperEngine/Data/Parsers/ObjectParser.cpp
    src/WallpaperEngine/Data/Parsers/ObjectParser.h
    src/WallpaperEngine/Data/Parsers/MaterialParser.cpp
    src/WallpaperEngine/Data/Parsers/MaterialParser.h
    src/WallpaperEngine/Data/Parsers/MdlParser.cpp
    src/WallpaperEngine/Data/Parsers/MdlParser.h
    src/WallpaperEngine/Data/Parsers/MdlAnimationParser.cpp
    src/WallpaperEngine/Data/Parsers/MdlAnimationParser.h
    src/WallpaperEngine/Data/Parsers/ModelParser.cpp
    src/WallpaperEngine/Data/Parsers/ModelParser.h
    src/WallpaperEngine/Data/Parsers/ShaderConstantParser.cpp
    src/WallpaperEngine/Data/Parsers/ShaderConstantParser.h
    src/WallpaperEngine/Data/Parsers/PackageParser.cpp
    src/WallpaperEngine/Data/Parsers/PackageParser.h
    src/WallpaperEngine/Data/Parsers/PropertyParser.cpp
    src/WallpaperEngine/Data/Parsers/PropertyParser.h
    src/WallpaperEngine/Data/Parsers/TextureParser.cpp
    src/WallpaperEngine/Data/Parsers/TextureParser.h
    src/WallpaperEngine/Data/Builders/ColorBuilder.h
    src/WallpaperEngine/Data/Builders/ColorBuilder.cpp
    src/WallpaperEngine/Data/Builders/UserSettingBuilder.h
    src/WallpaperEngine/Data/Builders/VectorBuilder.cpp
    src/WallpaperEngine/Data/Builders/VectorBuilder.h
    src/WallpaperEngine/Data/Dumpers/StringPrinter.cpp
    src/WallpaperEngine/Data/Dumpers/StringPrinter.h
    src/WallpaperEngine/Data/JSON.cpp

    src/WallpaperEngine/VideoPlayback/MPV/GLPlayer.cpp
    src/WallpaperEngine/VideoPlayback/MPV/GLPlayer.h
    src/WallpaperEngine/VideoPlayback/MPV/MemoryStreamProtocol.cpp
    src/WallpaperEngine/VideoPlayback/MPV/MemoryStreamProtocol.h

    src/WallpaperEngine/Scripting/Adapters/ObjectAdapter.h
    src/WallpaperEngine/Scripting/Adapters/ObjectAdapter.cpp
    src/WallpaperEngine/Scripting/Adapters/VectorAdapter.h
    src/WallpaperEngine/Scripting/Adapters/VectorAdapter.cpp
    src/WallpaperEngine/Scripting/Adapters/ScriptableObjectAdapter.h
    src/WallpaperEngine/Scripting/Adapters/ScriptableObjectAdapter.cpp
    src/WallpaperEngine/Scripting/Adapters/VideoTextureAdapter.cpp

    src/WallpaperEngine/Scripting/Modules/ColorModule.h
    src/WallpaperEngine/Scripting/Modules/ColorModule.cpp
    src/WallpaperEngine/Scripting/Modules/MathModule.h
    src/WallpaperEngine/Scripting/Modules/MathModule.cpp
    src/WallpaperEngine/Scripting/Modules/VectorModule.h
    src/WallpaperEngine/Scripting/Modules/VectorModule.cpp
    src/WallpaperEngine/Scripting/Modules/ScriptModule.h
    src/WallpaperEngine/Scripting/Modules/ScriptModule.cpp

    src/WallpaperEngine/Scripting/ConsoleObject.h
    src/WallpaperEngine/Scripting/ConsoleObject.cpp
    src/WallpaperEngine/Scripting/EngineObject.h
    src/WallpaperEngine/Scripting/EngineObject.cpp
    src/WallpaperEngine/Scripting/InputObject.h
    src/WallpaperEngine/Scripting/InputObject.cpp
    src/WallpaperEngine/Scripting/SceneObject.h
    src/WallpaperEngine/Scripting/SceneObject.cpp
    src/WallpaperEngine/Scripting/LocalStorage.h
    src/WallpaperEngine/Scripting/LocalStorage.cpp
    src/WallpaperEngine/Scripting/ScriptEngine.h
    src/WallpaperEngine/Scripting/ScriptPropertiesObject.h
    src/WallpaperEngine/Scripting/ScriptPropertiesObject.cpp
    src/WallpaperEngine/Scripting/ScriptEngine.cpp
    src/WallpaperEngine/Scripting/ScriptableObject.h
    src/WallpaperEngine/Scripting/ScriptableObject.cpp

    src/WallpaperEngine/Maths.h
    src/WallpaperEngine/Maths.cpp)

set(WPENGINE_COMMON_INPUTS ${COMMON_SOURCES})
# Tests omit desktop backends and always use ERRORONLY=0 and DEMOMODE=0.
# Wayland-only builds use different GLEW headers. CEF sets NDEBUG for Release
# engine objects, so reuse them only when the tests also compile with NDEBUG.
string(TOUPPER "${CMAKE_BUILD_TYPE};${CMAKE_CONFIGURATION_TYPES}" WPENGINE_BUILD_CONFIGS)
separate_arguments(WPENGINE_RELEASE_FLAGS NATIVE_COMMAND "${CMAKE_CXX_FLAGS_RELEASE}")
if(BUILD_TESTING AND CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SIZEOF_VOID_P EQUAL 8
        AND ERRORONLY STREQUAL "0" AND DEMOMODE STREQUAL "0" AND NOT DEFINED api_version
        AND (X11_SUPPORT_FOUND OR NOT WAYLAND_SUPPORT_FOUND)
        AND (NOT "RELEASE" IN_LIST WPENGINE_BUILD_CONFIGS OR "-DNDEBUG" IN_LIST WPENGINE_RELEASE_FLAGS))
    # GLFWOpenGLDriver references X11Output when ENABLE_X11 is defined.
    set(WPENGINE_GLFW_DRIVER src/WallpaperEngine/Render/Drivers/GLFWOpenGLDriver.cpp)
    set(WPENGINE_OBJECT_SOURCES ${COMMON_SOURCES})
    list(REMOVE_ITEM WPENGINE_OBJECT_SOURCES ${WPENGINE_GLFW_DRIVER})
    add_library(linux-wallpaperengine-objects OBJECT ${WPENGINE_OBJECT_SOURCES})
    set(WPENGINE_COMMON_INPUTS ${WPENGINE_GLFW_DRIVER} $<TARGET_OBJECTS:linux-wallpaperengine-objects>)
endif()

if(BUILD_TESTING)
    add_executable(
        tests

        ${WPENGINE_COMMON_INPUTS}

        src/WallpaperEngine/Testing/Cases/Example.cpp
        src/WallpaperEngine/Testing/Cases/ApplicationLanguage.cpp
        src/WallpaperEngine/Testing/Render/TestingOpenGLDriver.cpp
        src/WallpaperEngine/Testing/Render/TestingOpenGLDriver.h
        src/WallpaperEngine/Testing/Input/TestingMouseInput.cpp
        src/WallpaperEngine/Testing/Input/TestingMouseInput.h
        src/WallpaperEngine/Testing/Harnesses/RenderHarness.cpp
        src/WallpaperEngine/Testing/Harnesses/RenderHarness.h
        src/WallpaperEngine/Testing/Cases/MouseCoordinates.cpp
        src/WallpaperEngine/Testing/Cases/CameraPath.cpp
        src/WallpaperEngine/Testing/Cases/LightState.cpp
        src/WallpaperEngine/Testing/Cases/PropertyParser.cpp
        src/WallpaperEngine/Testing/Cases/LocalStorage.cpp
        src/WallpaperEngine/Testing/Cases/PlaylistRestoration.cpp
        src/WallpaperEngine/Testing/Cases/ScriptRuntime.cpp
        src/WallpaperEngine/Testing/Cases/Utf8Text.cpp
        src/WallpaperEngine/Testing/Cases/TextStyle.cpp
        src/WallpaperEngine/Testing/Cases/JsonTolerance.cpp
        src/WallpaperEngine/Testing/Cases/SceneMsaaPolicy.cpp
        src/WallpaperEngine/Testing/Cases/EffectConditions.cpp
        src/WallpaperEngine/Testing/Cases/EffectFunctions.cpp
        src/WallpaperEngine/Testing/Cases/FluidTargetFormats.cpp
        src/WallpaperEngine/Testing/Cases/EffectFBOBindings.cpp
        src/WallpaperEngine/Testing/Cases/TextureUpload.cpp
        src/WallpaperEngine/Testing/Cases/TextureDecode.cpp
        src/WallpaperEngine/Testing/Cases/TextureAnimationParsing.cpp
        src/WallpaperEngine/Testing/Cases/MdlParser.cpp
        src/WallpaperEngine/Testing/Cases/PuppetClippingPlan.cpp
        src/WallpaperEngine/Testing/Cases/MdlAnimation.cpp
        src/WallpaperEngine/Testing/Cases/MdlAnimationBounds.cpp
        src/WallpaperEngine/Testing/Cases/PuppetMorph.cpp
        src/WallpaperEngine/Testing/Cases/ModelPassOrder.cpp
	    src/WallpaperEngine/Testing/Cases/RenderQuality.cpp
        src/WallpaperEngine/Testing/Cases/ParticleSemantics.cpp
        src/WallpaperEngine/Testing/Cases/ParticleColorParsing.cpp
        src/WallpaperEngine/Testing/Cases/AudioSpectrum.cpp
        src/WallpaperEngine/Testing/Cases/AudioStreamLoop.cpp
        src/WallpaperEngine/Testing/Cases/SoundSpatial.cpp
        src/WallpaperEngine/Testing/Cases/ShaderIncludes.cpp
        src/WallpaperEngine/Testing/Cases/ShaderProgramCache.cpp
        src/WallpaperEngine/Testing/Cases/ShaderDiskCache.cpp
        src/WallpaperEngine/Testing/Cases/PackageLookup.cpp)
endif()

add_library(
    linux-wallpaperengine-lib SHARED

    ${WPENGINE_COMMON_INPUTS}
    ${WAYLAND_SOURCES}
    ${X11_SOURCES}
    ${DEMOMODE_SOURCES})

set(WPENGINE_ENGINE_LIBRARIES
    ${OPENGL_LIBRARIES}
    GLEW::GLEW
    ${GLUT_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${LZ4_LIBRARY}
    ${SDL2_LIBRARIES}
    ${FFMPEG_LIBRARIES}
    ${MPV_LIBRARY}
    ${PULSEAUDIO_LIBRARY}
    Freetype::Freetype
    msdfgen::msdfgen
    Fontconfig::Fontconfig
    PkgConfig::HARFBUZZ
    ${WAYLAND_LIBRARIES}
    ${X11_LIBRARIES}
    ${DBUS_LIBRARIES}
    kissfft
    glslang
    spirv-cross-core
    spirv-cross-glsl
    glfw
    libcef_lib
    libcef_dll_wrapper
    argparse
    ${WPENGINE_SCRIPT_RUNTIME}
    lwe-sound-spatial)

target_link_libraries(linux-wallpaperengine-lib PUBLIC ${WPENGINE_ENGINE_LIBRARIES})

if (BUILD_TESTING)
    target_link_libraries(tests PRIVATE Catch2::Catch2WithMain ${WPENGINE_ENGINE_LIBRARIES})
endif()

set(WPENGINE_ENGINE_DEFINITIONS ERRORONLY=${ERRORONLY} DEMOMODE=${DEMOMODE})

if (BUILD_TESTING)
    # tests should give as much output as possible
    target_compile_definitions(tests PRIVATE ERRORONLY=0)
    target_compile_definitions(tests PRIVATE DEMOMODE=0)
endif()

if(X11_SUPPORT_FOUND)
    list(APPEND WPENGINE_ENGINE_DEFINITIONS ENABLE_X11)

    # make sure some of the X11 functions we'll use are available
    check_function_exists(XSetIOErrorExitHandler HAVE_XSETIOERROREXITHANDLER)

    if(HAVE_XSETIOERROREXITHANDLER)
        list(APPEND WPENGINE_ENGINE_DEFINITIONS HAVE_XSETIOERROREXITHANDLER=1)
    endif()
endif()

if(WAYLAND_SUPPORT_FOUND)
    list(APPEND WPENGINE_ENGINE_DEFINITIONS ENABLE_WAYLAND)
endif()

if(WAYLAND_SUPPORT_FOUND AND NOT X11_SUPPORT_FOUND)
    list(APPEND WPENGINE_ENGINE_DEFINITIONS GLEW_EGL)
endif()

target_compile_definitions(linux-wallpaperengine-lib PUBLIC ${WPENGINE_ENGINE_DEFINITIONS})

set(WPENGINE_ENGINE_INCLUDE_DIRS
    src
    src/External/glslang-WallpaperEngine
    src/External/SPIRV-Cross-WallpaperEngine
    src/External/MimeTypes
    src/External/json/include
    src/External/stb
    src/External/kissfft
    ${MPV_INCLUDE_DIR}
    ${GLEW_INCLUDE_DIR}
    ${LZ4_INCLUDE_DIR}
    ${SDL2_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIR}
    ${PULSEAUDIO_INCLUDE_DIR}
    ${CEF_INCLUDE_PATH}
    ${CMAKE_SOURCE_DIR}
    ${X11_INCLUDES}
    ${WAYLAND_OUTPUT_DIR})

target_include_directories(linux-wallpaperengine-lib PUBLIC ${WPENGINE_ENGINE_INCLUDE_DIRS})

if(TARGET linux-wallpaperengine-objects)
    # Match the shared library's compilation while keeping the test executable's
    # own compile and link settings. Each final target links the dependencies.
    set_target_properties(linux-wallpaperengine-objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_compile_definitions(linux-wallpaperengine-objects PRIVATE
        ${WPENGINE_ENGINE_DEFINITIONS} linux_wallpaperengine_lib_EXPORTS)
    target_include_directories(linux-wallpaperengine-objects PRIVATE ${WPENGINE_ENGINE_INCLUDE_DIRS})
    target_link_libraries(linux-wallpaperengine-objects PRIVATE ${WPENGINE_ENGINE_LIBRARIES})
    REPLACED_SET_COMMON_TARGET_PROPERTIES(linux-wallpaperengine-objects)
endif()

COPY_FILES(linux-wallpaperengine-lib "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "${TARGET_OUTPUT_DIRECTORY}")
COPY_FILES(linux-wallpaperengine-lib "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${TARGET_OUTPUT_DIRECTORY}")
# remove the vulkan lib as chromium includes a broken libvulkan.so.1 with it
file(REMOVE "${CEF_BINARY_DIR}/libvulkan.so.1")
set_target_properties(linux-wallpaperengine-lib PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${TARGET_OUTPUT_DIRECTORY})

add_dependencies(linux-wallpaperengine-lib libcef_dll_wrapper glslang spirv-cross-core spirv-cross-glsl)
# custom version of cef's SET_EXECUTABLE_TARGET_PROPERTIES that fixes C files being compiled with the wrong parameters
REPLACED_SET_EXECUTABLE_TARGET_PROPERTIES(linux-wallpaperengine-lib)

add_executable(linux-wallpaperengine
    src/main.cpp)

target_link_libraries (linux-wallpaperengine PUBLIC linux-wallpaperengine-lib)

# CEF's prebuilt Linux library calls the legacy mallinfo() symbol. Export the
# compatibility implementation in main.cpp so its 32-bit counters cannot
# overflow inside Chromium's periodic memory dump.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_property(TARGET linux-wallpaperengine PROPERTY ENABLE_EXPORTS TRUE)
endif()

# CEF resolves icudtl.dat and its other runtime resources relative to the loaded libcef.so.
# Prefer the staged output directory over CEF's internal Release directory so the build-tree
# executable loads the library beside the resources copied by COPY_FILES. Keep the same portable
# layout after installation.
set_target_properties(linux-wallpaperengine linux-wallpaperengine-lib PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
)

# Install the main executable and shared library via install(TARGETS)
# so that CMake automatically strips/replaces the build-time RPATH.
install(TARGETS linux-wallpaperengine
    RUNTIME DESTINATION .
)
install(TARGETS linux-wallpaperengine-lib
    LIBRARY DESTINATION .
)

# Install everything else from the output directory (CEF resources, helper binaries, etc.)
# but exclude the files already handled by install(TARGETS) above.
install(DIRECTORY ${TARGET_OUTPUT_DIRECTORY}/ DESTINATION .
    PATTERN "linux-wallpaperengine" EXCLUDE
    PATTERN "liblinux-wallpaperengine-lib.so" EXCLUDE
)
