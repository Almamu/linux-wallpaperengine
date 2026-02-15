cmake_minimum_required(VERSION 3.12)
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
find_package(GLUT REQUIRED)
find_package(ZLIB REQUIRED)
find_package(SDL2 REQUIRED)
find_package(MPV REQUIRED)
find_package(LZ4 REQUIRED)
find_package(FFMPEG REQUIRED)
find_package(PulseAudio REQUIRED)

set(ENABLE_OPT OFF)
set(SPIRV_CROSS_FORCE_PIC ON)
set(KISSFFT_TEST OFF)
set(KISSFFT_TOOLS OFF)

# Download CEF of specified version for current platform
# Specify the CEF distribution version.
set(CEF_VERSION "135.0.17+gcbc1c5b+chromium-135.0.7049.52")
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
list(APPEND CEF_COMPILER_FLAGS -rdynamic)
list(APPEND CEF_EXE_LINKER_FLAGS -rdynamic)
# remove the vulkan library from the files to copy so it doesn't fail after removing it
list(REMOVE_ITEM CEF_BINARY_FILES libvulkan.so.1)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/output)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(TARGET_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/output)

add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)
add_subdirectory(src/External/glslang-WallpaperEngine glslang)
add_subdirectory(src/External/SPIRV-Cross-WallpaperEngine spirv-cross)
add_subdirectory(src/External/kissfft kissfft)
add_subdirectory(src/External/argparse argparse)
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
        COMMAND ${WaylandScanner} client-header ${CMAKE_SOURCE_DIR}/protocols/wlr-foreign-toplevel-management-unstable-v1.xml ${WAYLAND_OUTPUT_DIR}/wlr-foreign-toplevel-management-unstable-v1-protocol.h)
    add_custom_command(OUTPUT ${WAYLAND_OUTPUT_DIR}/wlr-foreign-toplevel-management-unstable-v1-protocol.c
        COMMAND ${WaylandScanner} private-code ${CMAKE_SOURCE_DIR}/protocols/wlr-foreign-toplevel-management-unstable-v1.xml ${WAYLAND_OUTPUT_DIR}/wlr-foreign-toplevel-management-unstable-v1-protocol.c)
    add_custom_command(OUTPUT ${WAYLAND_OUTPUT_DIR}/wlr-layer-shell-unstable-v1-protocol.h
        COMMAND ${WaylandScanner} client-header ${CMAKE_SOURCE_DIR}/protocols/wlr-layer-shell-unstable-v1.xml ${WAYLAND_OUTPUT_DIR}/wlr-layer-shell-unstable-v1-protocol.h)
    add_custom_command(OUTPUT ${WAYLAND_OUTPUT_DIR}/wlr-layer-shell-unstable-v1-protocol.c
        COMMAND ${WaylandScanner} private-code ${CMAKE_SOURCE_DIR}/protocols/wlr-layer-shell-unstable-v1.xml ${WAYLAND_OUTPUT_DIR}/wlr-layer-shell-unstable-v1-protocol.c)
    add_custom_command(OUTPUT ${WAYLAND_OUTPUT_DIR}/xdg-shell-protocol.h
        COMMAND ${WaylandScanner} client-header ${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml ${WAYLAND_OUTPUT_DIR}/xdg-shell-protocol.h)
    add_custom_command(OUTPUT ${WAYLAND_OUTPUT_DIR}/xdg-shell-protocol.c
        COMMAND ${WaylandScanner} private-code ${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml ${WAYLAND_OUTPUT_DIR}/xdg-shell-protocol.c)

    include_directories(${WAYLAND_SUPPORT_INCLUDE_DIRS})
    include_directories(${WAYLAND_OUTPUT_DIR})
    set(WAYLAND_LIBRARIES
        pthread
        wayland-cursor
        wayland-client
        wayland-egl
        ${OPENGL_egl_LIBRARY})
    set(WAYLAND_SOURCES
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
        ${WAYLAND_OUTPUT_DIR}/wlr-layer-shell-unstable-v1-protocol.h)
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

include_directories(
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
    ${X11_INCLUDES})

set(COMMON_SOURCES
    src/External/MimeTypes/MimeTypes.cpp
    src/External/MimeTypes/MimeTypes.h

    src/Steam/FileSystem/FileSystem.cpp
    src/Steam/FileSystem/FileSystem.h

    src/WallpaperEngine/Logging/Log.cpp
    src/WallpaperEngine/Logging/Log.h

    src/WallpaperEngine/Debugging/CallStack.cpp
    src/WallpaperEngine/Debugging/CallStack.h

    src/WallpaperEngine/Application/ApplicationContext.cpp
    src/WallpaperEngine/Application/ApplicationContext.h
    src/WallpaperEngine/Application/WallpaperApplication.cpp
    src/WallpaperEngine/Application/WallpaperApplication.h

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

    src/WallpaperEngine/Audio/Drivers/Recorders/PulseAudioPlaybackRecorder.cpp
    src/WallpaperEngine/Audio/Drivers/Recorders/PulseAudioPlaybackRecorder.h
    src/WallpaperEngine/Audio/Drivers/Recorders/PlaybackRecorder.cpp
    src/WallpaperEngine/Audio/Drivers/Recorders/PlaybackRecorder.h

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
    src/WallpaperEngine/Audio/AudioStream.cpp
    src/WallpaperEngine/Audio/AudioStream.h

    src/WallpaperEngine/Input/InputContext.cpp
    src/WallpaperEngine/Input/InputContext.h
    src/WallpaperEngine/Input/MouseInput.h
    src/WallpaperEngine/Input/Drivers/GLFWMouseInput.cpp
    src/WallpaperEngine/Input/Drivers/GLFWMouseInput.h

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
    src/WallpaperEngine/Render/Shaders/ShaderUnit.cpp
    src/WallpaperEngine/Render/Shaders/ShaderUnit.h
    src/WallpaperEngine/Render/Shaders/GLSLContext.cpp
    src/WallpaperEngine/Render/Shaders/GLSLContext.h

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
    src/WallpaperEngine/Render/CWallpaper.cpp
    src/WallpaperEngine/Render/WallpaperState.h
    src/WallpaperEngine/Render/WallpaperState.cpp
    src/WallpaperEngine/Render/Wallpapers/CScene.h
    src/WallpaperEngine/Render/Wallpapers/CScene.cpp
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
    src/WallpaperEngine/Render/TextureProvider.h

    src/WallpaperEngine/Render/Objects/CRenderable.cpp
    src/WallpaperEngine/Render/Objects/CRenderable.h
    src/WallpaperEngine/Render/Objects/CImage.h
    src/WallpaperEngine/Render/Objects/CImage.cpp
    src/WallpaperEngine/Render/Objects/CSound.h
    src/WallpaperEngine/Render/Objects/CSound.cpp
    src/WallpaperEngine/Render/Objects/CParticle.h
    src/WallpaperEngine/Render/Objects/CParticle.cpp

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
    src/WallpaperEngine/Data/Model/UserSetting.h
    src/WallpaperEngine/Data/Model/DynamicValue.h
    src/WallpaperEngine/Data/Model/DynamicValue.cpp
    src/WallpaperEngine/Data/Model/Property.h
    src/WallpaperEngine/Data/Utils/TypeCaster.h
    src/WallpaperEngine/Data/Utils/BinaryReader.cpp
    src/WallpaperEngine/Data/Utils/BinaryReader.h
    src/WallpaperEngine/Data/Utils/MemoryStream.h
    src/WallpaperEngine/Data/Utils/SFINAE.h
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
    src/WallpaperEngine/Data/Builders/UserSettingBuilder.h
    src/WallpaperEngine/Data/Builders/VectorBuilder.cpp
    src/WallpaperEngine/Data/Builders/VectorBuilder.h
    src/WallpaperEngine/Data/Dumpers/StringPrinter.cpp
    src/WallpaperEngine/Data/Dumpers/StringPrinter.h
    src/WallpaperEngine/Data/JSON.cpp)

if(BUILD_TESTING)
    add_executable(
        tests

        ${COMMON_SOURCES}

        src/WallpaperEngine/Testing/Cases/Example.cpp
        src/WallpaperEngine/Testing/Render/TestingOpenGLDriver.cpp
        src/WallpaperEngine/Testing/Render/TestingOpenGLDriver.h
        src/WallpaperEngine/Testing/Input/TestingMouseInput.cpp
        src/WallpaperEngine/Testing/Input/TestingMouseInput.h
        src/WallpaperEngine/Testing/Harnesses/RenderHarness.cpp
        src/WallpaperEngine/Testing/Harnesses/RenderHarness.h
        src/WallpaperEngine/Testing/Cases/MouseCoordinates.cpp)
endif()

add_executable(
    linux-wallpaperengine
    src/main.cpp

    ${COMMON_SOURCES}
    ${WAYLAND_SOURCES}
    ${X11_SOURCES}
    ${DEMOMODE_SOURCES})

target_link_libraries (linux-wallpaperengine PUBLIC
    ${OPENGL_LIBRARIES}
    GLEW::GLEW
    ${GLUT_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${LZ4_LIBRARY}
    ${SDL2_LIBRARIES}
    ${FFMPEG_LIBRARIES}
    ${MPV_LIBRARY}
    ${PULSEAUDIO_LIBRARY}
    ${WAYLAND_LIBRARIES}
    ${X11_LIBRARIES}
    kissfft
    glslang
    spirv-cross-core
    spirv-cross-glsl
    glfw
    libcef_lib
    libcef_dll_wrapper
    argparse)

if (BUILD_TESTING)
    target_link_libraries (tests PRIVATE
        Catch2::Catch2WithMain
        ${OPENGL_LIBRARIES}
        GLEW::GLEW
        ${GLUT_LIBRARIES}
        ${ZLIB_LIBRARIES}
        ${LZ4_LIBRARY}
        ${SDL2_LIBRARIES}
        ${FFMPEG_LIBRARIES}
        ${MPV_LIBRARY}
        ${PULSEAUDIO_LIBRARY}
        ${WAYLAND_LIBRARIES}
        ${X11_LIBRARIES}
        kissfft
        glslang
        spirv-cross-core
        spirv-cross-glsl
        glfw
        libcef_lib
        libcef_dll_wrapper
        argparse)
endif()

target_compile_definitions(linux-wallpaperengine PUBLIC ERRORONLY=${ERRORONLY})
target_compile_definitions(linux-wallpaperengine PUBLIC DEMOMODE=${DEMOMODE})

if (BUILD_TESTING)
    # tests should give as much output as possible
    target_compile_definitions(tests PRIVATE ERRORONLY=0)
    target_compile_definitions(tests PRIVATE DEMOMODE=0)
endif()

if(X11_SUPPORT_FOUND)
    target_compile_definitions(linux-wallpaperengine PUBLIC ENABLE_X11)

    # make sure some of the X11 functions we'll use are available
    check_function_exists(XSetIOErrorExitHandler HAVE_XSETIOERROREXITHANDLER)

    if(HAVE_XSETIOERROREXITHANDLER)
        target_compile_definitions(linux-wallpaperengine PUBLIC HAVE_XSETIOERROREXITHANDLER=1)
    endif()
endif()

if(WAYLAND_SUPPORT_FOUND)
    target_compile_definitions(linux-wallpaperengine PUBLIC ENABLE_WAYLAND)
endif()

COPY_FILES(linux-wallpaperengine "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "${TARGET_OUTPUT_DIRECTORY}")
COPY_FILES(linux-wallpaperengine "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${TARGET_OUTPUT_DIRECTORY}")
# remove the vulkan lib as chromium includes a broken libvulkan.so.1 with it
file(REMOVE "${CEF_BINARY_DIR}/libvulkan.so.1")

add_dependencies(linux-wallpaperengine libcef_dll_wrapper glslang spirv-cross-core spirv-cross-glsl)
# custom version of cef's SET_EXECUTABLE_TARGET_PROPERTIES that fixes C files being compiled with the wrong parameters
REPLACED_SET_EXECUTABLE_TARGET_PROPERTIES(linux-wallpaperengine)

# set some install parameters if not in debug mode
install(DIRECTORY ${TARGET_OUTPUT_DIRECTORY}/ DESTINATION .)
install(FILES ${TARGET_OUTPUT_DIRECTORY}/${PROJECT_NAME} PERMISSIONS OWNER_READ OWNER_WRITE WORLD_EXECUTE WORLD_READ GROUP_READ DESTINATION .)
