include(SetupCEF)
find_package(ZLIB REQUIRED)
find_package(SDL2 REQUIRED)
find_package(MPV REQUIRED)
find_package(LZ4 REQUIRED)
find_package(FFMPEG REQUIRED)
include(SetupExternals)

add_executable(linux-wallpaperengine-webhelper
    ${COMMON_SOURCES}

    WallpaperEngine/WebBrowser/WebBrowserHelper.cpp)

target_include_directories(linux-wallpaperengine-webhelper PRIVATE
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

target_link_libraries(linux-wallpaperengine-webhelper PRIVATE
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

COPY_FILES(linux-wallpaperengine-webhelper "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "${TARGET_OUTPUT_DIRECTORY}")
COPY_FILES(linux-wallpaperengine-webhelper "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${TARGET_OUTPUT_DIRECTORY}")

set_target_properties(linux-wallpaperengine-webhelper PROPERTIES
    SKIP_BUILD_RPATH FALSE
    BUILD_WITH_INSTALL_RPATH FALSE
    INSTALL_RPATH "${CMAKE_INSTALL_FULL_LIBDIR};${CMAKE_INSTALL_FULL_LIBDIR}/linux-wallpaperengine"
    INSTALL_RPATH_USE_LINK_PATH TRUE)

add_dependencies(linux-wallpaperengine-webhelper libcef_dll_wrapper kissfft::kissfft-${KISSFFT_DATATYPE} glslang spirv-cross-core spirv-cross-glsl qjs)

SET_EXECUTABLE_TARGET_PROPERTIES(linux-wallpaperengine-webhelper)

# set some install parameters if not in debug mode
install(TARGETS linux-wallpaperengine-webhelper
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/linux-wallpaperengine)
