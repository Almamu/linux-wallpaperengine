set(TARGET_NAME linux-wallpaperengine-webhelper)
set(TARGET_SOURCE_FILES
    $<TARGET_OBJECTS:linux-wallpaperengine-common-object>
    WallpaperEngine/WebBrowser/WebBrowserHelper.cpp
    WallpaperEngine/WebBrowser/CEF/BrowserApp.cpp
    WallpaperEngine/WebBrowser/CEF/BrowserClient.cpp
    WallpaperEngine/WebBrowser/CEF/RenderHandler.cpp
    WallpaperEngine/WebBrowser/CEF/WPSchemeHandler.cpp
    WallpaperEngine/WebBrowser/CEF/WPSchemeHandlerFactory.cpp
    WallpaperEngine/WebBrowser/IPC/BrowserServer.cpp)
set(TARGET_LIBRARIES
    libcef_lib
    libcef_dll_wrapper
    linux-wallpaperengine-common-object)

add_library(${TARGET_NAME}-object OBJECT ${TARGET_SOURCE_FILES})
target_include_directories(${TARGET_NAME}-object PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(${TARGET_NAME}-object PRIVATE
    WPENGINE_CEF_RESOURCES_PATH="${CMAKE_INSTALL_FULL_LIBDIR}/linux-wallpaperengine")

if(NOT WPBUILD_WEBHELPER)
    return()
endif()

include(RequireCEF)
target_link_libraries(${TARGET_NAME}-object PRIVATE ${TARGET_LIBRARIES})

add_executable(${TARGET_NAME} $<TARGET_OBJECTS:${TARGET_NAME}-object>)
target_link_libraries(${TARGET_NAME} PRIVATE ${TARGET_LIBRARIES})

target_compile_definitions(${TARGET_NAME} PRIVATE
    WPENGINE_CEF_RESOURCES_PATH="${CMAKE_INSTALL_FULL_LIBDIR}/linux-wallpaperengine")

COPY_FILES(${TARGET_NAME} "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "${TARGET_OUTPUT_DIRECTORY}")
COPY_FILES(${TARGET_NAME} "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${TARGET_OUTPUT_DIRECTORY}")

set_target_properties(${TARGET_NAME} PROPERTIES
    SKIP_BUILD_RPATH FALSE
    BUILD_RPATH "${TARGET_OUTPUT_DIRECTORY}"
    BUILD_WITH_INSTALL_RPATH FALSE
    INSTALL_RPATH "${CMAKE_INSTALL_FULL_LIBDIR};${CMAKE_INSTALL_FULL_LIBDIR}/linux-wallpaperengine"
    INSTALL_RPATH_USE_LINK_PATH TRUE)

add_dependencies(${TARGET_NAME} libcef_dll_wrapper)

SET_EXECUTABLE_TARGET_PROPERTIES(${TARGET_NAME}-object)

install(TARGETS ${TARGET_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/linux-wallpaperengine)
