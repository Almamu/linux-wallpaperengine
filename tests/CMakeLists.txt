set(TARGET_NAME linux-wallpaperengine-tests)
set(TARGET_SOURCE_FILES
    $<TARGET_OBJECTS:linux-wallpaperengine-core-object>

    Tests/Library/Playlists.cpp
    Tests/Library/Backgrounds.cpp
    Tests/MouseCoordinates.cpp)
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
    glfw
    qjs)

add_library(${TARGET_NAME}-object OBJECT ${TARGET_SOURCE_FILES})
target_include_directories(${TARGET_NAME}-object PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT WPBUILD_TESTS)
    return()
endif()

include(RequireGLSLANG)
include(RequireKissFFT)
include(RequireCEF)
include(RequireQuickJS)
include(RequireNLOHMANNJson)
include(RequireCatch2)
pkg_check_modules(lz4 REQUIRED IMPORTED_TARGET liblz4)
pkg_check_modules(zlib REQUIRED IMPORTED_TARGET zlib)
pkg_check_modules(mpv REQUIRED IMPORTED_TARGET mpv)
pkg_check_modules(ffmpeg REQUIRED IMPORTED_TARGET libavformat libavcodec libswresample libswscale libavutil)
find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2)

target_link_libraries(${TARGET_NAME}-object PRIVATE ${TARGET_LIBRARIES}
    Catch2::Catch2)

add_executable(${TARGET_NAME} $<TARGET_OBJECTS:${TARGET_NAME}-object>)
target_link_libraries(${TARGET_NAME} PRIVATE ${TARGET_NAME}-object ${TARGET_LIBRARIES} Catch2::Catch2WithMain)

SET_EXECUTABLE_TARGET_PROPERTIES(${TARGET_NAME}-object)

# tests should give as much output as possible
target_compile_definitions(${TARGET_NAME} PRIVATE ERRORONLY=0)
