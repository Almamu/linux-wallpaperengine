set(TARGET_NAME linux-wallpaperengine-common)
set(TARGET_SOURCE_FILES
    ${EXTERNAL_DEPS_DIR}/MimeTypes/MimeTypes.cpp
    WallpaperEngine/Logging/Log.cpp
    WallpaperEngine/Debugging/CallStack.cpp
    WallpaperEngine/Utils/UUID.cpp
    Steam/FileSystem/FileSystem.cpp)

add_library(${TARGET_NAME}-object OBJECT ${TARGET_SOURCE_FILES})
target_include_directories(${TARGET_NAME}-object PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})