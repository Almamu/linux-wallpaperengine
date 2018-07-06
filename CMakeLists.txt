cmake_minimum_required(VERSION 3.0)
project(wallengine)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-fpermissive")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")

find_package(X11 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Irrlicht REQUIRED)

include_directories(${X11_INCLUDE_DIR} ${IRRLICHT_INCLUDE_DIR} .)

add_executable(wallengine main.cpp common.h nier_test.cpp wallpaperengine/shaders/compiler.h wallpaperengine/shaders/compiler.cpp nier_test.h wallpaperengine/project.cpp wallpaperengine/project.h wallpaperengine/scene.cpp wallpaperengine/scene.h wallpaperengine/object.cpp wallpaperengine/object.h wallpaperengine/camera.cpp wallpaperengine/camera.h wallpaperengine/core.cpp wallpaperengine/core.h wallpaperengine/image.cpp wallpaperengine/image.h wallpaperengine/object3d.cpp wallpaperengine/object3d.h wallpaperengine/effect.cpp wallpaperengine/effect.h wallpaperengine/material.cpp wallpaperengine/material.h wallpaperengine/fs/fileResolver.cpp wallpaperengine/fs/fileResolver.h wallpaperengine/irrlicht.cpp wallpaperengine/irrlicht.h wallpaperengine/config.cpp wallpaperengine/config.h)

target_link_libraries(wallengine ${X11_LIBRARIES} ${X11_Xxf86vm_LIB} ${OPENGL_LIBRARIES} ${GLUT_LIBRARIES} ${ZLIB_LIBRARIES} ${IRRLICHT_LIBRARY})