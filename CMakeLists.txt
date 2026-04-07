cmake_minimum_required(VERSION 3.12)
project(linux-wallpaperengine)

# prevent installing to /usr by default so we don't bork anything
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/opt/${PROJECT_NAME}" CACHE PATH "..." FORCE)
endif()

option(WPBUILD_CORE "Toggles building the core library (on by default)" ON)
option(WPBUILD_WEBHELPER "Toggles building the webhelper (on by default)" ON)
option(WPBUILD_FRONTEND "Toggles building the default frontend (off by default)" OFF)
option(WPBUILD_DEVVIEWER "Toggles building the dev-viewer for easier debugging (off by default)" OFF)
option(WPBUILD_TESTS "Toggles building the tests (off by default)" OFF)

set_property(GLOBAL PROPERTY OS_FOLDERS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(include)

# obtain last version tag
execute_process(
    COMMAND git describe --tags --dirty --always
    OUTPUT_VARIABLE PROJECT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE)

# get version off the tag
string(REGEX MATCH "^v?([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ ${PROJECT_VERSION})

if(CMAKE_MATCH_0)
    set(PROJECT_VERSION
            "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}"
    )

    set(PROJECT_VERSION_MAJOR "${CMAKE_MATCH_1}")
    set(PROJECT_VERSION_MINOR "${CMAKE_MATCH_2}")
    set(PROJECT_VERSION_PATCH "${CMAKE_MATCH_3}")
    set(PROJECT_SOVERSION "${PROJECT_VERSION_MAJOR}")
else()
    message(WARNING "No valid tag found, falling back to 0.0.0")
    set(PROJECT_VERSION "0.0.0")
    set(PROJECT_SOVERSION "0")
endif()

# if you're developing you might find this debug option useful for shader output, although RenderDoc is encouraged
if(NOT ERRORONLY)
    set(ERRORONLY 0)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# include debug symbols even in release
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-narrowing")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/output)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(TARGET_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/output)

set(COMMON_SOURCES
    ${CMAKE_SOURCE_DIR}/src/core/frontends/configuration.cpp
    ${CMAKE_SOURCE_DIR}/src/core/frontends/content.cpp
    ${CMAKE_SOURCE_DIR}/src/core/frontends/project.cpp
    ${CMAKE_SOURCE_DIR}/src/core/frontends/context.cpp
    ${CMAKE_SOURCE_DIR}/src/core/frontends/render.cpp
    ${CMAKE_SOURCE_DIR}/src/core/frontends/playlists.cpp

    ${CMAKE_SOURCE_DIR}/src/External/MimeTypes/MimeTypes.cpp

    ${CMAKE_SOURCE_DIR}/src/core/Steam/FileSystem/FileSystem.cpp

    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Logging/Log.cpp

    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Debugging/CallStack.cpp

    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Assets/AssetLoadException.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Assets/AssetLocator.cpp

    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/FileSystem/Container.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/FileSystem/Adapters/Directory.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/FileSystem/Adapters/Package.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/FileSystem/Adapters/Virtual.cpp

    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Audio/Drivers/AudioDriver.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Audio/Drivers/SDLAudioDriver.cpp

    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Audio/PlaybackRecorder.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Audio/AudioPlayingDetector.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Audio/AudioContext.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Audio/AudioStream.cpp

    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Render/Shaders/Variables/ShaderVariable.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Render/Shaders/Variables/ShaderVariableFloat.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Render/Shaders/Variables/ShaderVariableInteger.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Render/Shaders/Variables/ShaderVariableVector2.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Render/Shaders/Variables/ShaderVariableVector3.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Render/Shaders/Variables/ShaderVariableVector4.cpp

    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Render/Shaders/Shader.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Render/Shaders/ShaderUnit.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Render/Shaders/GLSLContext.cpp

    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Render/Helpers/ContextAware.cpp

    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Render/RenderContext.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Render/TextureCache.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Render/FBOProvider.cpp

    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Render/Helpers/ContextAware.cpp

    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Render/CWallpaper.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Render/Wallpapers/CScene.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Render/Wallpapers/CVideo.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Render/Wallpapers/CWeb.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Render/Camera.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Render/CObject.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Render/CTexture.cpp

    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Render/Objects/CRenderable.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Render/Objects/CImage.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Render/Objects/CSound.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Render/Objects/CParticle.cpp

    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Render/CFBO.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Render/Objects/Effects/CPass.cpp

    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/WebBrowser/CEF/RenderHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/WebBrowser/CEF/BrowserClient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/WebBrowser/CEF/BrowserApp.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/WebBrowser/CEF/SubprocessApp.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/WebBrowser/CEF/WPSchemeHandlerFactory.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/WebBrowser/CEF/WPSchemeHandler.cpp

    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Data/Model/DynamicValue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Data/Model/ScriptedDynamicValue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Data/Utils/BinaryReader.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Data/Parsers/EffectParser.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Data/Parsers/ProjectParser.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Data/Parsers/WallpaperParser.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Data/Parsers/UserSettingParser.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Data/Parsers/ObjectParser.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Data/Parsers/MaterialParser.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Data/Parsers/ModelParser.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Data/Parsers/ShaderConstantParser.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Data/Parsers/PackageParser.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Data/Parsers/PropertyParser.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Data/Parsers/TextureParser.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Data/Parsers/ConfigParser.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Data/Builders/VectorBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Data/Dumpers/StringPrinter.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Data/JSON.cpp

    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/VideoPlayback/MPV/GLPlayer.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/VideoPlayback/MPV/MemoryStreamProtocol.cpp

    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Scripting/ScriptEngine.cpp

    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Utils/UUID.cpp

    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Configuration.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Context.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WallpaperEngine/Project.cpp)

add_subdirectory(src/glad linux-wallpaperengine-glad)

if(WPBUILD_CORE)
add_subdirectory(src/core linux-wallpaperengine-core)
endif()

if(WPBUILD_DEVVIEWER)
add_subdirectory(src/dev-viewer linux-wallpaperengine-dev-viewer)
endif()

if(WPBUILD_FRONTEND)
#add_subdirectory(src/frontend)
endif()

if(WPBUILD_WEBHELPER)
add_subdirectory(src/webhelper linux-wallpaperengine-webhelper)
endif()

if(WPBUILD_TESTS)
add_subdirectory(tests tests)
endif()