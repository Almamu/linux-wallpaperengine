cmake_minimum_required(VERSION 3.0)
project(wallengine)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-fpermissive")

find_package(X11 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)
find_package(ZLIB REQUIRED)

add_library(irrlicht STATIC IMPORTED)
set_property(TARGET irrlicht PROPERTY IMPORTED_LOCATION ../../lib/irrlicht/lib/Linux/libIrrlicht.a)

include_directories(${X11_INCLUDE_DIR} "../lib/irrlicht/include")

add_executable(wallengine main.cpp common.h nier_test.cpp BasicShaderLoader.h BasicShaderLoader.cpp nier_test.h)

target_link_libraries(wallengine ${X11_LIBRARIES} ${X11_Xxf86vm_LIB} ${OPENGL_LIBRARIES} ${GLUT_LIBRARIES} ${ZLIB_LIBRARIES} irrlicht)