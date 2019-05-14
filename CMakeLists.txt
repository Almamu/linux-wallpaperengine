cmake_minimum_required(VERSION 3.0)
project(wallengine)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-fpermissive")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")
set(OpenGL_GL_PREFERENCE "LEGACY")

find_package(X11 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Irrlicht REQUIRED)
find_package(LZ4 REQUIRED)

include_directories(${X11_INCLUDE_DIR} ${IRRLICHT_INCLUDE_DIR} ${LZ4_INCLUDE_DIR} .)

add_executable(wallengine main.cpp wallpaperengine/shaders/compiler.h wallpaperengine/shaders/compiler.cpp wallpaperengine/project.cpp wallpaperengine/project.h wallpaperengine/scene.cpp wallpaperengine/scene.h wallpaperengine/object.cpp wallpaperengine/object.h wallpaperengine/camera.cpp wallpaperengine/camera.h wallpaperengine/core.cpp wallpaperengine/core.h wallpaperengine/image.cpp wallpaperengine/image.h wallpaperengine/object3d.cpp wallpaperengine/object3d.h wallpaperengine/effect.cpp wallpaperengine/effect.h wallpaperengine/fs/utils.cpp wallpaperengine/fs/utils.h wallpaperengine/irrlicht.cpp wallpaperengine/irrlicht.h wallpaperengine/video/renderer.cpp wallpaperengine/video/renderer.h wallpaperengine/video/node.cpp wallpaperengine/video/node.h wallpaperengine/video/material.cpp wallpaperengine/video/material.h wallpaperengine/texture.cpp wallpaperengine/texture.h wallpaperengine/irr/CImageLoaderTEX.h wallpaperengine/irr/CImageLoaderTEX.cpp wallpaperengine/irr/CPkgReader.h wallpaperengine/irr/CPkgReader.cpp wallpaperengine/irr/CFileList.h wallpaperengine/irr/CFileList.cpp wallpaperengine/irr/vector4d.h)

target_link_libraries(wallengine ${X11_LIBRARIES} ${X11_Xxf86vm_LIB} ${OPENGL_LIBRARIES} ${GLUT_LIBRARIES} ${ZLIB_LIBRARIES} ${IRRLICHT_LIBRARY} ${LZ4_LIBRARY})