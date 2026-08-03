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
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(EXTERNAL_DEPS_DIR ${CMAKE_SOURCE_DIR}/src/External)

# obtain last version tag
execute_process(
    COMMAND git describe --tags --dirty --always
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE)

message(NOTICE "Repository version ${GIT_TAG}")

# get version off the tag
string(REGEX MATCH "^v?([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ ${GIT_TAG})

if(CMAKE_MATCH_0)
    set(PROJECT_VERSION "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")

    set(PROJECT_VERSION_MAJOR "${CMAKE_MATCH_1}")
    set(PROJECT_VERSION_MINOR "${CMAKE_MATCH_2}")
    set(PROJECT_VERSION_PATCH "${CMAKE_MATCH_3}")
    set(PROJECT_SOVERSION "${PROJECT_VERSION_MAJOR}")
else()
    message(FATAL_ERROR "Cannot determine version")
endif()

# if you're developing you might find this debug option useful for shader output, although RenderDoc is encouraged
if(NOT ERRORONLY)
    set(ERRORONLY 0)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(PkgConfig REQUIRED)

# include debug symbols even in release
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-narrowing")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/output)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(TARGET_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/output)

add_subdirectory(src/glad linux-wallpaperengine-glad)
add_subdirectory(src/common linux-wallpaperengine-common)
add_subdirectory(src/core linux-wallpaperengine-core)
add_subdirectory(src/dev-viewer linux-wallpaperengine-dev-viewer)
add_subdirectory(src/frontend linux-wallpaperengine)
add_subdirectory(src/webhelper linux-wallpaperengine-webhelper)
add_subdirectory(tests linux-wallpaperengine-tests)