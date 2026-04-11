set(TARGET_NAME linux-wallpaperengine)
set(TARGET_SOURCE_FILES
    $<TARGET_OBJECTS:linux-wallpaperengine-common-object>
    WallpaperEngine/Application/ApplicationContext.cpp
    WallpaperEngine/Application/WallpaperApplication.cpp
    WallpaperEngine/Audio/Pulseaudio.cpp
    WallpaperEngine/Desktop/Universal/GLFW.cpp
    WallpaperEngine/Desktop/Output.cpp

    main.cpp)
set(TARGET_LIBRARIES
    linux-wallpaperengine-common-object
    linux-wallpaperengine-glad
    linux-wallpaperengine-core
    PkgConfig::pulseaudio
    glfw
    argparse)

if(NOT WPBUILD_FRONTEND)
    return()
endif()

include(RequireArgParse)
find_package(linux-wallpaperengine-core REQUIRED)
find_package(glfw3 REQUIRED)
pkg_check_modules(pulseaudio REQUIRED IMPORTED_TARGET libpulse)

add_executable(${TARGET_NAME} ${TARGET_SOURCE_FILES})

target_include_directories(${TARGET_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${EXTERNAL_DEPS_DIR}/stb)

target_link_libraries (${TARGET_NAME} PUBLIC ${TARGET_LIBRARIES})
