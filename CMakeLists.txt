cmake_minimum_required(VERSION 3.12)
project(linux-wallpaperengine)

# prevent installing to /usr by default so we don't bork anything
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/opt/${PROJECT_NAME}" CACHE PATH "..." FORCE)
endif()

set_property(GLOBAL PROPERTY OS_FOLDERS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# if you're developing you might find this debug option useful for shader output, although RenderDoc is encouraged
if(NOT ERRORONLY)
    set(ERRORONLY 0)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-narrowing")

find_package(ZLIB REQUIRED)
find_package(SDL2 REQUIRED)
find_package(MPV REQUIRED)
find_package(LZ4 REQUIRED)
find_package(FFMPEG REQUIRED)
find_package(PulseAudio REQUIRED)

option(ENABLE_OPT OFF)

# quickjs build
option(QJS_ENABLE_INSTALL OFF)

# glslang build
option(BUILD_SHARED_LIBS OFF)
option(ENABLE_GLSLANG_BINARIES OFF)
option(GLSLANG_TESTS OFF)
option(GLSLANG_ENABLE_INSTALL OFF)
option(ENABLE_GLSLANG_JS OFF)
option(ENABLE_HLSL OFF)

# configure spirv to not output any files
# just build as static library
option(SPIRV_CROSS_FORCE_PIC ON)
option(SPIRV_CROSS_CLI OFF)
# need to force this because otherwise it wont work
set(SPIRV_CROSS_SKIP_INSTALL ON CACHE BOOL "Skips installation targets." FORCE)
option(SPIRV_CROSS_ENABLE_TESTS OFF)
option(SPIRV_CROSS_ENABLE_HLSL OFF)
option(SPIRV_CROSS_ENABLE_MSL OFF)
option(SPIRV_CROSS_ENABLE_REFLECT OFF)
option(SPIRV_CROSS_ENABLE_C_API OFF)
option(SPIRV_CROSS_ENABLE_UTIL OFF)
option(SPIRV_CROSS_ENABLE_REFLECT OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# configure kissfft build to static link and no files generation
option(KISSFFT_TEST OFF)
option(KISSFFT_TOOLS OFF)
option(KISSFFT_PKGCONFIG OFF)
# need to force this because otherwise it wont work
set(KISSFFT_STATIC ON CACHE BOOL "Build kissfft as static (ON) or shared library (OFF)" FORCE)
option(KISSFFT_ENABLE_INSTALL OFF)

# Download CEF of specified version for current platform
# Specify the CEF distribution version.
set(CEF_VERSION "145.0.28+g51162e8+chromium-145.0.7632.160")
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
add_subdirectory(src/External/kissfft-WallpaperEngine kissfft)
add_subdirectory(src/External/argparse argparse)
add_subdirectory(src/External/quickjs-WallpaperEngine quickjs)
add_subdirectory(src/External/glad linux-wallpaperengine-glad)
if(BUILD_TESTING)
    add_subdirectory(src/External/Catch2)
endif()

add_library(ceflib SHARED IMPORTED)
set_target_properties(ceflib
        PROPERTIES IMPORTED_LOCATION ${TARGET_OUTPUT_DIRECTORY}/libcef.so)

ADD_LOGICAL_TARGET(libcef_lib "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")
PRINT_CEF_CONFIG()

include_directories(
    include
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
    ${CMAKE_SOURCE_DIR})

set(COMMON_SOURCES
    include/frontends/configuration.h
    src/frontends/configuration.cpp
    include/frontends/project.h
    src/frontends/project.cpp
    include/frontends/context.h
    src/frontends/context.cpp
    include/frontends/pulseaudio.h
    include/frontends/render.h
    src/frontends/render.cpp

    src/External/MimeTypes/MimeTypes.cpp
    src/External/MimeTypes/MimeTypes.h

    src/Steam/FileSystem/FileSystem.cpp
    src/Steam/FileSystem/FileSystem.h

    src/WallpaperEngine/Context.h

    src/WallpaperEngine/Logging/Log.cpp
    src/WallpaperEngine/Logging/Log.h

    src/WallpaperEngine/Debugging/CallStack.cpp
    src/WallpaperEngine/Debugging/CallStack.h

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

    src/WallpaperEngine/Input/MouseClickStatus.h

    src/WallpaperEngine/Audio/Drivers/Recorders/PulseAudioPlaybackRecorder.cpp
    src/WallpaperEngine/Audio/Drivers/Recorders/PulseAudioPlaybackRecorder.h
    src/WallpaperEngine/Audio/Drivers/Recorders/PlaybackRecorder.cpp
    src/WallpaperEngine/Audio/Drivers/Recorders/PlaybackRecorder.h

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

    src/WallpaperEngine/Render/Drivers/Detectors/FullScreenDetector.cpp
    src/WallpaperEngine/Render/Drivers/Detectors/FullScreenDetector.h

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
    src/WallpaperEngine/Data/JSON.cpp

    src/WallpaperEngine/VideoPlayback/MPV/GLPlayer.cpp
    src/WallpaperEngine/VideoPlayback/MPV/GLPlayer.h
    src/WallpaperEngine/VideoPlayback/MPV/MemoryStreamProtocol.cpp
    src/WallpaperEngine/VideoPlayback/MPV/MemoryStreamProtocol.h

    src/WallpaperEngine/Utils/UUID.h
    src/WallpaperEngine/Utils/UUID.cpp

    src/WallpaperEngine/Configuration.h
    src/WallpaperEngine/Context.h
    src/WallpaperEngine/Scripting/ScriptEngine.h
    src/WallpaperEngine/Scripting/ScriptEngine.cpp
    src/WallpaperEngine/Data/Model/ScriptedDynamicValue.h
    src/WallpaperEngine/Data/Model/ScriptedDynamicValue.cpp)

if(BUILD_TESTING)
    add_executable(
        tests

        ${COMMON_SOURCES}

        src/WallpaperEngine/Testing/Cases/Example.cpp
        src/WallpaperEngine/Testing/Cases/MouseCoordinates.cpp)
endif()

add_library(
    linux-wallpaperengine-core SHARED

    ${COMMON_SOURCES})

add_executable(linux-wallpaperengine-webhelper
    src/WallpaperEngine/Utils/UUID.cpp
    src/WallpaperEngine/WebBrowser/WebBrowserHelper.cpp)

target_link_libraries(linux-wallpaperengine-webhelper PUBLIC
    linux-wallpaperengine-core
    linux-wallpaperengine-glad
    libcef_lib
    libcef_dll_wrapper)

target_include_directories(linux-wallpaperengine-core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_compile_definitions(linux-wallpaperengine-core PRIVATE
    WPENGINE_WEBHELPER_PATH="${CMAKE_INSTALL_FULL_BINDIR}/linux-wallpaperengine-webhelper"
    WPENGINE_CEF_RESOURCES_PATH="${CMAKE_INSTALL_FULL_LIBDIR}/linux-wallpaperengine")

set_target_properties(linux-wallpaperengine-webhelper PROPERTIES
    SKIP_BUILD_RPATH FALSE
    BUILD_WITH_INSTALL_RPATH FALSE
    INSTALL_RPATH "${CMAKE_INSTALL_FULL_LIBDIR};${CMAKE_INSTALL_FULL_LIBDIR}/linux-wallpaperengine"
    INSTALL_RPATH_USE_LINK_PATH TRUE)

set_target_properties(linux-wallpaperengine-core PROPERTIES
    SKIP_BUILD_RPATH FALSE
    BUILD_WITH_INSTALL_RPATH FALSE
    INSTALL_RPATH "${CMAKE_INSTALL_FULL_LIBDIR};${CMAKE_INSTALL_FULL_LIBDIR}/linux-wallpaperengine"
    INSTALL_RPATH_USE_LINK_PATH TRUE)

target_link_libraries (linux-wallpaperengine-core PUBLIC
    $<BUILD_INTERFACE:linux-wallpaperengine-glad>
    $<BUILD_INTERFACE:${ZLIB_LIBRARIES}>
    $<BUILD_INTERFACE:${LZ4_LIBRARY}>
    $<BUILD_INTERFACE:${SDL2_LIBRARIES}>
    $<BUILD_INTERFACE:${FFMPEG_LIBRARIES}>
    $<BUILD_INTERFACE:${MPV_LIBRARY}>
    $<BUILD_INTERFACE:${PULSEAUDIO_LIBRARY}>
    $<BUILD_INTERFACE:kissfft::kissfft-${KISSFFT_DATATYPE}>
    $<BUILD_INTERFACE:glslang>
    $<BUILD_INTERFACE:spirv-cross-core>
    $<BUILD_INTERFACE:spirv-cross-glsl>
    $<BUILD_INTERFACE:libcef_lib>
    $<BUILD_INTERFACE:libcef_dll_wrapper>
    $<BUILD_INTERFACE:argparse>
    $<BUILD_INTERFACE:qjs>)

if (BUILD_TESTING)
    target_link_libraries (tests PRIVATE
        Catch2::Catch2WithMain
        linux-wallpaperengine-glad
        ${GLUT_LIBRARIES}
        ${ZLIB_LIBRARIES}
        ${LZ4_LIBRARY}
        ${SDL2_LIBRARIES}
        ${FFMPEG_LIBRARIES}
        ${MPV_LIBRARY}
        ${PULSEAUDIO_LIBRARY}
        kissfft::kissfft-${KISSFFT_DATATYPE}
        glslang
        spirv-cross-core
        spirv-cross-glsl
        libcef_lib
        libcef_dll_wrapper
        argparse
        qjs)
endif()

target_compile_definitions(linux-wallpaperengine-core PUBLIC ERRORONLY=${ERRORONLY})

if (BUILD_TESTING)
    # tests should give as much output as possible
    target_compile_definitions(tests PRIVATE ERRORONLY=0)
endif()

COPY_FILES(linux-wallpaperengine-core "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "${TARGET_OUTPUT_DIRECTORY}")
COPY_FILES(linux-wallpaperengine-core "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${TARGET_OUTPUT_DIRECTORY}")
# remove the vulkan lib as chromium includes a broken libvulkan.so.1 with it
file(REMOVE "${CEF_BINARY_DIR}/libvulkan.so.1")
set_target_properties(linux-wallpaperengine-core PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${TARGET_OUTPUT_DIRECTORY})

add_dependencies(linux-wallpaperengine-core libcef_dll_wrapper kissfft::kissfft-${KISSFFT_DATATYPE} glslang spirv-cross-core spirv-cross-glsl qjs)
SET_EXECUTABLE_TARGET_PROPERTIES(linux-wallpaperengine-core)

# set some install parameters if not in debug mode
install(TARGETS linux-wallpaperengine-core linux-wallpaperengine-webhelper
    EXPORT linux-wallpaperengine-core
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/linux-wallpaperengine)

install(DIRECTORY ${TARGET_OUTPUT_DIRECTORY}/ DESTINATION "${CMAKE_INSTALL_LIBDIR}/linux-wallpaperengine"
    FILES_MATCHING PATTERN "*.so*" PATTERN "*.bin" PATTERN "*.dat" PATTERN "*.pak" PATTERN "locales/*")

install(DIRECTORY include/frontends/ DESTINATION include/${PROJECT_NAME})

# Export targets
install(EXPORT linux-wallpaperengine-core
    NAMESPACE linux-wallpaperengine-core
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/linux-wallpaperengine-core)

# pkg-config generation
if(NOT PROJECT_VERSION)
    set(PROJECT_VERSION "0.0.1")
endif()

configure_package_config_file(linux-wallpaperengine-core-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/linux-wallpaperengine-core-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/linux-wallpaperengine-core)

write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/linux-wallpaperengine-core-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/linux-wallpaperengine-core-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/linux-wallpaperengine-core-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/linux-wallpaperengine-core)

configure_file(linux-wallpaperengine-core.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/linux-wallpaperengine-core.pc
    @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/linux-wallpaperengine-core.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
