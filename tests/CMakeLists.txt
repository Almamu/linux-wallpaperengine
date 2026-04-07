include(SetupCEF)
find_package(ZLIB REQUIRED)
find_package(SDL2 REQUIRED)
find_package(MPV REQUIRED)
find_package(LZ4 REQUIRED)
find_package(FFMPEG REQUIRED)
include(SetupExternals)

add_subdirectory(${CMAKE_SOURCE_DIR}/src/External/Catch2 catch2)

add_executable(tests
    ${COMMON_SOURCES}

    Tests/Library/Playlists.cpp
    Tests/Library/Backgrounds.cpp
    Tests/MouseCoordinates.cpp)

target_include_directories(tests PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/core>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/External/glslang-WallpaperEngine>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/External/SPIRV-Cross-WallpaperEngine>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/External/MimeTypes>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/External/json/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/External/stb>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/External/kissfft-WallpaperEngine>
    $<BUILD_INTERFACE:${MPV_INCLUDE_DIR}>
    $<BUILD_INTERFACE:${LZ4_INCLUDE_DIR}>
    $<BUILD_INTERFACE:${SDL2_INCLUDE_DIRS}>
    $<BUILD_INTERFACE:${FFMPEG_INCLUDE_DIR}>
    $<BUILD_INTERFACE:${CEF_INCLUDE_PATH}>
    $<INSTALL_INTERFACE:include>)

target_link_libraries (tests PRIVATE
    Catch2::Catch2WithMain
    $<BUILD_INTERFACE:linux-wallpaperengine-glad>
    $<BUILD_INTERFACE:${ZLIB_LIBRARIES}>
    $<BUILD_INTERFACE:${LZ4_LIBRARY}>
    $<BUILD_INTERFACE:${SDL2_LIBRARIES}>
    $<BUILD_INTERFACE:${FFMPEG_LIBRARIES}>
    $<BUILD_INTERFACE:${MPV_LIBRARY}>
    $<BUILD_INTERFACE:kissfft::kissfft-${KISSFFT_DATATYPE}>
    $<BUILD_INTERFACE:glslang>
    $<BUILD_INTERFACE:spirv-cross-core>
    $<BUILD_INTERFACE:spirv-cross-glsl>
    $<BUILD_INTERFACE:libcef_lib>
    $<BUILD_INTERFACE:libcef_dll_wrapper>
    $<BUILD_INTERFACE:qjs>)

# tests should give as much output as possible
target_compile_definitions(tests PRIVATE ERRORONLY=0)
