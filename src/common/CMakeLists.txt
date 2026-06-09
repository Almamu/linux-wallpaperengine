set(TARGET_NAME linux-wallpaperengine-common)
set(TARGET_SOURCE_FILES
    ${EXTERNAL_DEPS_DIR}/MimeTypes/MimeTypes.cpp
    ${EXTERNAL_DEPS_DIR}/source-parsers/packages/source-parsers-shared/internal/case-insensitive-map.cpp
    ${EXTERNAL_DEPS_DIR}/source-parsers/packages/source-parsers-shared/internal/offset-data-view.cpp
    ${EXTERNAL_DEPS_DIR}/source-parsers/packages/source-parsers-shared/internal/peekable-stream.cpp
    ${EXTERNAL_DEPS_DIR}/source-parsers/packages/vdfparser/keyvalue.cpp
    ${EXTERNAL_DEPS_DIR}/source-parsers/packages/vdfparser/vdf.cpp
    WallpaperEngine/Logging/Log.cpp
    WallpaperEngine/Debugging/CallStack.cpp
    WallpaperEngine/Utils/UUID.cpp
    Steam/FileSystem/FileSystem.cpp)

add_library(${TARGET_NAME}-object OBJECT ${TARGET_SOURCE_FILES})
target_include_directories(${TARGET_NAME}-object PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${EXTERNAL_DEPS_DIR}/source-parsers/packages
    ${EXTERNAL_DEPS_DIR}/source-parsers/packages/vdfparser)