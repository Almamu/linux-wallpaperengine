cmake_minimum_required(VERSION 3.12)
project(linux-wallpaperengine-glad)

add_library(linux-wallpaperengine-glad)

target_sources(linux-wallpaperengine-glad PRIVATE src/glad.c)

target_include_directories(linux-wallpaperengine-glad PUBLIC include)
find_package(OpenGL REQUIRED)

target_link_libraries(linux-wallpaperengine-glad PUBLIC ${OpenGL_LIBRARIES})