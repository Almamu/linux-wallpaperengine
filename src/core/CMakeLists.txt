include(SetupCEF)
find_package(ZLIB REQUIRED)
find_package(SDL2 REQUIRED)
find_package(MPV REQUIRED)
find_package(LZ4 REQUIRED)
find_package(FFMPEG REQUIRED)
include(SetupExternals)

# generate the version file that will be exported with the installation
configure_file(
    version.h.in
    ${CMAKE_BINARY_DIR}/generated/version.h)

add_library(linux-wallpaperengine-core SHARED
    ${COMMON_SOURCES})

target_compile_definitions(linux-wallpaperengine-core PRIVATE
    WPENGINE_WEBHELPER_PATH="${CMAKE_INSTALL_FULL_BINDIR}/linux-wallpaperengine-webhelper"
    WPENGINE_CEF_RESOURCES_PATH="${CMAKE_INSTALL_FULL_LIBDIR}/linux-wallpaperengine")

target_compile_definitions(linux-wallpaperengine-core PRIVATE WPENGINE_BUILD)
target_compile_definitions(linux-wallpaperengine-core PUBLIC ERRORONLY=${ERRORONLY})

target_include_directories(linux-wallpaperengine-core PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
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

target_link_options(linux-wallpaperengine-core PRIVATE
    "-Wl,--exclude-libs,ALL"
    "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/version.lds")

target_link_libraries(linux-wallpaperengine-core PRIVATE
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

COPY_FILES(linux-wallpaperengine-core "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "${TARGET_OUTPUT_DIRECTORY}")
COPY_FILES(linux-wallpaperengine-core "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${TARGET_OUTPUT_DIRECTORY}")

set_target_properties(linux-wallpaperengine-core PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${TARGET_OUTPUT_DIRECTORY}
    SKIP_BUILD_RPATH FALSE
    BUILD_WITH_INSTALL_RPATH FALSE
    INSTALL_RPATH "${CMAKE_INSTALL_FULL_LIBDIR};${CMAKE_INSTALL_FULL_LIBDIR}/linux-wallpaperengine"
    INSTALL_RPATH_USE_LINK_PATH TRUE
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_SOVERSION})

add_dependencies(linux-wallpaperengine-core libcef_dll_wrapper kissfft::kissfft-${KISSFFT_DATATYPE} glslang spirv-cross-core spirv-cross-glsl qjs)

SET_LIBRARY_TARGET_PROPERTIES(linux-wallpaperengine-core)

install(TARGETS linux-wallpaperengine-core
    EXPORT linux-wallpaperengine-core
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/linux-wallpaperengine)

install(DIRECTORY ${TARGET_OUTPUT_DIRECTORY}/ DESTINATION "${CMAKE_INSTALL_LIBDIR}/linux-wallpaperengine"
    FILES_MATCHING PATTERN "*.so*" PATTERN "*.bin" PATTERN "*.dat" PATTERN "*.pak" PATTERN "locales/*")

install(DIRECTORY ${CMAKE_BINARY_DIR}/generated/ DESTINATION include/${PROJECT_NAME})
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/frontends/ DESTINATION include/${PROJECT_NAME})

# Export targets
install(EXPORT linux-wallpaperengine-core
    NAMESPACE linux-wallpaperengine-core
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/linux-wallpaperengine-core)

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
