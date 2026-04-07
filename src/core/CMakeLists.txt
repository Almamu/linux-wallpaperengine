set(TARGET_NAME linux-wallpaperengine-core)
set(TARGET_SOURCE_FILES
    $<TARGET_OBJECTS:linux-wallpaperengine-common-object>

    frontends/configuration.cpp
    frontends/content.cpp
    frontends/project.cpp
    frontends/context.cpp
    frontends/render.cpp
    frontends/playlists.cpp

    WallpaperEngine/Debugging/CallStack.cpp

    WallpaperEngine/Assets/AssetLoadException.cpp
    WallpaperEngine/Assets/AssetLocator.cpp

    WallpaperEngine/FileSystem/Container.cpp
    WallpaperEngine/FileSystem/Adapters/Directory.cpp
    WallpaperEngine/FileSystem/Adapters/Package.cpp
    WallpaperEngine/FileSystem/Adapters/Virtual.cpp

    WallpaperEngine/Audio/Drivers/AudioDriver.cpp
    WallpaperEngine/Audio/Drivers/SDLAudioDriver.cpp

    WallpaperEngine/Audio/PlaybackRecorder.cpp
    WallpaperEngine/Audio/AudioPlayingDetector.cpp
    WallpaperEngine/Audio/AudioContext.cpp
    WallpaperEngine/Audio/AudioStream.cpp

    WallpaperEngine/Render/Shaders/Variables/ShaderVariable.cpp
    WallpaperEngine/Render/Shaders/Variables/ShaderVariableFloat.cpp
    WallpaperEngine/Render/Shaders/Variables/ShaderVariableInteger.cpp
    WallpaperEngine/Render/Shaders/Variables/ShaderVariableVector2.cpp
    WallpaperEngine/Render/Shaders/Variables/ShaderVariableVector3.cpp
    WallpaperEngine/Render/Shaders/Variables/ShaderVariableVector4.cpp

    WallpaperEngine/Render/Shaders/Shader.cpp
    WallpaperEngine/Render/Shaders/ShaderUnit.cpp
    WallpaperEngine/Render/Shaders/GLSLContext.cpp

    WallpaperEngine/Render/Helpers/ContextAware.cpp

    WallpaperEngine/Render/RenderContext.cpp
    WallpaperEngine/Render/TextureCache.cpp
    WallpaperEngine/Render/FBOProvider.cpp

    WallpaperEngine/Render/Helpers/ContextAware.cpp

    WallpaperEngine/Render/CWallpaper.cpp
    WallpaperEngine/Render/Wallpapers/CScene.cpp
    WallpaperEngine/Render/Wallpapers/CVideo.cpp
    WallpaperEngine/Render/Wallpapers/CWeb.cpp
    WallpaperEngine/Render/Camera.cpp
    WallpaperEngine/Render/CObject.cpp
    WallpaperEngine/Render/CTexture.cpp

    WallpaperEngine/Render/Objects/CRenderable.cpp
    WallpaperEngine/Render/Objects/CImage.cpp
    WallpaperEngine/Render/Objects/CSound.cpp
    WallpaperEngine/Render/Objects/CParticle.cpp

    WallpaperEngine/Render/CFBO.cpp
    WallpaperEngine/Render/Objects/Effects/CPass.cpp

    WallpaperEngine/WebBrowser/CEF/RenderHandler.cpp
    WallpaperEngine/WebBrowser/CEF/BrowserClient.cpp
    WallpaperEngine/WebBrowser/CEF/BrowserApp.cpp
    WallpaperEngine/WebBrowser/CEF/SubprocessApp.cpp
    WallpaperEngine/WebBrowser/CEF/WPSchemeHandlerFactory.cpp
    WallpaperEngine/WebBrowser/CEF/WPSchemeHandler.cpp

    WallpaperEngine/Data/Model/DynamicValue.cpp
    WallpaperEngine/Data/Model/ScriptedDynamicValue.cpp
    WallpaperEngine/Data/Utils/BinaryReader.cpp
    WallpaperEngine/Data/Parsers/EffectParser.cpp
    WallpaperEngine/Data/Parsers/ProjectParser.cpp
    WallpaperEngine/Data/Parsers/WallpaperParser.cpp
    WallpaperEngine/Data/Parsers/UserSettingParser.cpp
    WallpaperEngine/Data/Parsers/ObjectParser.cpp
    WallpaperEngine/Data/Parsers/MaterialParser.cpp
    WallpaperEngine/Data/Parsers/ModelParser.cpp
    WallpaperEngine/Data/Parsers/ShaderConstantParser.cpp
    WallpaperEngine/Data/Parsers/PackageParser.cpp
    WallpaperEngine/Data/Parsers/PropertyParser.cpp
    WallpaperEngine/Data/Parsers/TextureParser.cpp
    WallpaperEngine/Data/Parsers/ConfigParser.cpp
    WallpaperEngine/Data/Builders/VectorBuilder.cpp
    WallpaperEngine/Data/Dumpers/StringPrinter.cpp
    WallpaperEngine/Data/JSON.cpp

    WallpaperEngine/VideoPlayback/MPV/GLPlayer.cpp
    WallpaperEngine/VideoPlayback/MPV/MemoryStreamProtocol.cpp

    WallpaperEngine/Scripting/ScriptEngine.cpp

    WallpaperEngine/Configuration.cpp
    WallpaperEngine/Context.cpp
    WallpaperEngine/Project.cpp)
set(TARGET_LIBRARIES
    linux-wallpaperengine-common-object
    linux-wallpaperengine-glad
    nlohmann_json::nlohmann_json
    PkgConfig::zlib
    PkgConfig::lz4
    SDL2::SDL2
    PkgConfig::ffmpeg
    PkgConfig::mpv
    kissfft::kissfft
    glslang
    spirv-cross-core
    spirv-cross-glsl
    libcef_lib
    libcef_dll_wrapper
    qjs)

add_library(${TARGET_NAME}-object OBJECT ${TARGET_SOURCE_FILES})
target_include_directories(${TARGET_NAME}-object PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${EXTERNAL_DEPS_DIR}/MimeTypes
    ${EXTERNAL_DEPS_DIR}/stb)

if (NOT WPBUILD_CORE)
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

# generate the version file that will be exported with the installation
configure_file(
    version.h.in
    ${CMAKE_BINARY_DIR}/generated/version.h)

add_library(${TARGET_NAME} SHARED $<TARGET_OBJECTS:${TARGET_NAME}-object>)

target_link_libraries(${TARGET_NAME}-object PRIVATE ${TARGET_LIBRARIES})
target_link_libraries(${TARGET_NAME} PRIVATE ${TARGET_LIBRARIES})

target_compile_definitions(${TARGET_NAME} PRIVATE
    WPENGINE_WEBHELPER_PATH="${CMAKE_INSTALL_FULL_BINDIR}/linux-wallpaperengine-webhelper"
    WPENGINE_CEF_RESOURCES_PATH="${CMAKE_INSTALL_FULL_LIBDIR}/linux-wallpaperengine")

target_compile_definitions(${TARGET_NAME} PRIVATE WPENGINE_BUILD)
target_compile_definitions(${TARGET_NAME} PUBLIC ERRORONLY=${ERRORONLY})

target_link_options(${TARGET_NAME} PRIVATE
    "-Wl,--exclude-libs,ALL"
    "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/version.lds")

COPY_FILES(${TARGET_NAME} "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "${TARGET_OUTPUT_DIRECTORY}")
COPY_FILES(${TARGET_NAME} "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${TARGET_OUTPUT_DIRECTORY}")

set_target_properties(${TARGET_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${TARGET_OUTPUT_DIRECTORY}
    SKIP_BUILD_RPATH FALSE
    BUILD_WITH_INSTALL_RPATH FALSE
    INSTALL_RPATH "${CMAKE_INSTALL_FULL_LIBDIR};${CMAKE_INSTALL_FULL_LIBDIR}/linux-wallpaperengine"
    INSTALL_RPATH_USE_LINK_PATH TRUE
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_SOVERSION})

add_dependencies(${TARGET_NAME}
    libcef_dll_wrapper
    kissfft::kissfft
    glslang
    spirv-cross-core
    spirv-cross-glsl
    qjs
    linux-wallpaperengine-glad)

SET_LIBRARY_TARGET_PROPERTIES(${TARGET_NAME}-object)

install(TARGETS ${TARGET_NAME}
    EXPORT ${TARGET_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/linux-wallpaperengine)

install(DIRECTORY ${TARGET_OUTPUT_DIRECTORY}/ DESTINATION "${CMAKE_INSTALL_LIBDIR}/linux-wallpaperengine"
    FILES_MATCHING PATTERN "*.so*" PATTERN "*.bin" PATTERN "*.dat" PATTERN "*.pak" PATTERN "locales/*")

install(DIRECTORY ${CMAKE_BINARY_DIR}/generated/ DESTINATION include/${PROJECT_NAME})
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/frontends/ DESTINATION include/${PROJECT_NAME})

# Export targets
install(EXPORT ${TARGET_NAME}
    NAMESPACE ${TARGET_NAME}
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET_NAME})

configure_package_config_file(linux-wallpaperengine-core-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/linux-wallpaperengine-core-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET_NAME})

write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/linux-wallpaperengine-core-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/linux-wallpaperengine-core-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/linux-wallpaperengine-core-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET_NAME})

configure_file(linux-wallpaperengine-core.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/linux-wallpaperengine-core.pc
    @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/linux-wallpaperengine-core.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
