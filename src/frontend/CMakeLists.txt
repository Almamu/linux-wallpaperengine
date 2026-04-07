set(TARGET_NAME linux-wallpaperengine)
set(TARGET_SOURCE_FILES
    $<TARGET_OBJECTS:linux-wallpaperengine-common-object>
    WallpaperEngine/Application/ApplicationContext.cpp
    WallpaperEngine/Application/WallpaperApplication.cpp

    main.cpp)

if(NOT WPBUILD_FRONTEND)
    return()
endif()

include(RequireArgParse)
find_package(linux-wallpaperengine-core REQUIRED)

add_executable(${TARGET_NAME} ${TARGET_SOURCE_FILES})

target_link_libraries (${TARGET_NAME} PUBLIC
    linux-wallpaperengine-core
    argparse)
