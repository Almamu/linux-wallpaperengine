set(TARGET_NAME linux-wallpaperengine-dev-viewer)
set(TARGET_SOURCE_FILES
    $<TARGET_OBJECTS:linux-wallpaperengine-common-object>
    $<TARGET_OBJECTS:linux-wallpaperengine-core-object>
    WallpaperEngine/Development/SimpleBackgroundViewer.cpp)
set(TARGET_LIBRARIES
    linux-wallpaperengine-common-object
    linux-wallpaperengine-core-object
    linux-wallpaperengine-glad
    nlohmann_json::nlohmann_json
    PkgConfig::zlib
    PkgConfig::lz4
    SDL2::SDL2
    PkgConfig::ffmpeg
    PkgConfig::mpv
    kissfft
    glslang
    spirv-cross-core
    spirv-cross-glsl
    libcef_lib
    libcef_dll_wrapper
    argparse
    glfw
    qjs)

add_library(${TARGET_NAME}-object OBJECT ${TARGET_SOURCE_FILES})
target_include_directories(${TARGET_NAME}-object PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT WPBUILD_DEVVIEWER)
    return()
endif()

include(RequireGLSLANG)
include(RequireKissFFT)
include(RequireCEF)
include(RequireQuickJS)
include(RequireArgParse)
find_package(PkgConfig REQUIRED)
pkg_check_modules(lz4 REQUIRED IMPORTED_TARGET liblz4)
pkg_check_modules(zlib REQUIRED IMPORTED_TARGET zlib)
pkg_check_modules(mpv REQUIRED IMPORTED_TARGET mpv)
pkg_check_modules(ffmpeg REQUIRED IMPORTED_TARGET libavformat libavcodec libswresample libswscale libavutil)
find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2)
find_package(glfw3 REQUIRED)

target_link_libraries(${TARGET_NAME}-object PRIVATE ${TARGET_LIBRARIES})

# linux-wallpaperengine-dev-viewer looks to simplify testing and validation of new features
# so a full, static build is better in this case
add_executable(${TARGET_NAME} $<TARGET_OBJECTS:${TARGET_NAME}-object>)

target_link_libraries(${TARGET_NAME} PRIVATE ${TARGET_LIBRARIES})

add_dependencies(${TARGET_NAME}
    libcef_dll_wrapper
    kissfft
    glslang
    spirv-cross-core
    spirv-cross-glsl
    qjs
    linux-wallpaperengine-glad)

SET_EXECUTABLE_TARGET_PROPERTIES(${TARGET_NAME}-object)
