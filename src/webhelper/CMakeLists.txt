set(TARGET_NAME linux-wallpaperengine-webhelper)
set(TARGET_SOURCE_FILES
    $<TARGET_OBJECTS:linux-wallpaperengine-core-object>
    WallpaperEngine/WebBrowser/WebBrowserHelper.cpp)
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
    qjs)

add_library(${TARGET_NAME}-object OBJECT ${TARGET_SOURCE_FILES})
target_include_directories(${TARGET_NAME}-object PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT WPBUILD_WEBHELPER)
    return()
endif()

include(RequireGLSLANG)
include(RequireKissFFT)
include(RequireCEF)
include(RequireQuickJS)
include(RequireNLOHMANNJson)
pkg_check_modules(lz4 REQUIRED IMPORTED_TARGET liblz4)
pkg_check_modules(zlib REQUIRED IMPORTED_TARGET zlib)
pkg_check_modules(mpv REQUIRED IMPORTED_TARGET mpv)
pkg_check_modules(ffmpeg REQUIRED IMPORTED_TARGET libavformat libavcodec libswresample libswscale libavutil)
find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2)

target_link_libraries(${TARGET_NAME}-object PRIVATE ${TARGET_LIBRARIES})

add_executable(${TARGET_NAME} $<TARGET_OBJECTS:${TARGET_NAME}-object>)
target_link_libraries(${TARGET_NAME} PRIVATE ${TARGET_LIBRARIES})

COPY_FILES(${TARGET_NAME} "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "${TARGET_OUTPUT_DIRECTORY}")
COPY_FILES(${TARGET_NAME} "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${TARGET_OUTPUT_DIRECTORY}")

set_target_properties(${TARGET_NAME} PROPERTIES
    SKIP_BUILD_RPATH FALSE
    BUILD_WITH_INSTALL_RPATH FALSE
    INSTALL_RPATH "${CMAKE_INSTALL_FULL_LIBDIR};${CMAKE_INSTALL_FULL_LIBDIR}/linux-wallpaperengine"
    INSTALL_RPATH_USE_LINK_PATH TRUE)

add_dependencies(${TARGET_NAME} libcef_dll_wrapper kissfft glslang spirv-cross-core spirv-cross-glsl qjs)

SET_EXECUTABLE_TARGET_PROPERTIES(${TARGET_NAME}-object)

# set some install parameters if not in debug mode
install(TARGETS ${TARGET_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/linux-wallpaperengine)
