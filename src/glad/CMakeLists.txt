set(TARGET_NAME linux-wallpaperengine-glad)
set(GLAD_SOURCE_FILES
    src/glad.c)

add_library(${TARGET_NAME} STATIC ${GLAD_SOURCE_FILES})
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(OpenGL REQUIRED)

target_link_libraries(${TARGET_NAME} PUBLIC ${OpenGL_LIBRARIES})