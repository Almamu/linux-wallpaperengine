# make sure to only create the targets once
# as multiple projects depend on it
if(NOT TARGET libcef_dll_wrapper)
    # Download CEF of specified version for current platform
    # Specify the CEF distribution version.
    set(CEF_VERSION "145.0.28+g51162e8+chromium-145.0.7632.160")
    set(CEF_DISTRIBUTION_TYPE "standard")
    # Determine the platform.
    if("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
        if("${PROJECT_ARCH}" STREQUAL "arm64")
            set(CEF_PLATFORM "macosarm64")
            set(CEF_ARCH_DETECTION "${PROJECT_ARCH}")
        elseif("${PROJECT_ARCH}" STREQUAL "x86_64")
            set(CEF_PLATFORM "macosx64")
            set(CEF_ARCH_DETECTION "${PROJECT_ARCH}")
        elseif("${CMAKE_HOST_SYSTEM_PROCESSOR}" STREQUAL "arm64")
            set(PROJECT_ARCH "arm64")
            set(CEF_PLATFORM "macosarm64")
            set(CEF_ARCH_DETECTION "${CMAKE_HOST_SYSTEM_PROCESSOR}")
        else()
            set(PROJECT_ARCH "x86_64")
            set(CEF_PLATFORM "macosx64")
            set(CEF_ARCH_DETECTION "Default for Darwin")
        endif()
    elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
        if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "arm")
            set(CEF_PLATFORM "linuxarm")
            set(CEF_ARCH_DETECTION "${CMAKE_SYSTEM_PROCESSOR}")
        elseif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "arm64")
            set(PROJECT_ARCH "arm64")
            set(CEF_PLATFORM "linuxarm64")
            set(CEF_ARCH_DETECTION "${CMAKE_SYSTEM_PROCESSOR}")
        elseif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "aarch64")
            set(PROJECT_ARCH "arm64")
            set(CEF_PLATFORM "linuxarm64")
            set(CEF_ARCH_DETECTION "${CMAKE_SYSTEM_PROCESSOR}")
        elseif(CMAKE_SIZEOF_VOID_P MATCHES 8)
            set(CEF_PLATFORM "linux64")
            set(CEF_ARCH_DETECTION "Based on void size ${CMAKE_SIZEOF_VOID_P} (${CMAKE_SYSTEM_PROCESSOR})")
        else()
            message(FATAL_ERROR "Linux x86 32-bit builds are discontinued.")
        endif()
    elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
        if("${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}" STREQUAL "ARM64")
            set(CEF_PLATFORM "windowsarm64")
            set(CEF_ARCH_DETECTION "${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}")
        elseif(CMAKE_SIZEOF_VOID_P MATCHES 8)
            set(CEF_PLATFORM "windows64")
            set(CEF_ARCH_DETECTION "Based on void size ${CMAKE_SIZEOF_VOID_P}")
        else()
            set(CEF_PLATFORM "windows32")
            set(CEF_ARCH_DETECTION "Default for Windows")
        endif()
    endif()

    if(CEF_DISTRIBUTION_TYPE AND NOT CEF_DISTRIBUTION_TYPE STREQUAL "standard")
        set(CEF_PLATFORM "${CEF_PLATFORM}_${CEF_DISTRIBUTION_TYPE}")
    endif()
    message(STATUS "Using CEF for ${CMAKE_SYSTEM_NAME} - ${CEF_PLATFORM} (${CEF_ARCH_DETECTION})")
    include(DownloadCEF)
    DownloadCEF("${CEF_PLATFORM}" "${CEF_VERSION}" "${CMAKE_BINARY_DIR}/cef")

    # add cef's cmake files to the lookup so we can use those directly
    list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")

    find_package(CEF REQUIRED)

    # remove some switches that forbid us from using things that are commonplace in our codebase
    list(REMOVE_ITEM CEF_CXX_COMPILER_FLAGS -fno-exceptions)
    list(REMOVE_ITEM CEF_CXX_COMPILER_FLAGS -fno-rtti)
    list(REMOVE_ITEM CEF_CXX_COMPILER_FLAGS -std=c++17)
    list(APPEND CEF_CXX_COMPILER_FLAGS -std=c++20)
    list(APPEND CEF_EXE_LINKER_FLAGS -rdynamic)
    # remove the vulkan library from the files to copy so it doesn't fail after removing it
    list(REMOVE_ITEM CEF_BINARY_FILES libvulkan.so.1)
    add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

    add_library(ceflib SHARED IMPORTED)
    set_target_properties(ceflib
            PROPERTIES IMPORTED_LOCATION ${TARGET_OUTPUT_DIRECTORY}/libcef.so)

    ADD_LOGICAL_TARGET(libcef_lib "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")
    PRINT_CEF_CONFIG()

    # remove the vulkan lib as chromium includes a broken libvulkan.so.1 with it
    file(REMOVE "${CEF_BINARY_DIR}/libvulkan.so.1")
else()
    # add cef's cmake files to the lookup so we can use those directly
    list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")

    find_package(CEF REQUIRED)

    # remove some switches that forbid us from using things that are commonplace in our codebase
    list(REMOVE_ITEM CEF_CXX_COMPILER_FLAGS -fno-exceptions)
    list(REMOVE_ITEM CEF_CXX_COMPILER_FLAGS -fno-rtti)
    list(REMOVE_ITEM CEF_CXX_COMPILER_FLAGS -std=c++17)
    list(APPEND CEF_CXX_COMPILER_FLAGS -std=c++20)
    list(APPEND CEF_EXE_LINKER_FLAGS -rdynamic)
    # remove the vulkan library from the files to copy so it doesn't fail after removing it
    list(REMOVE_ITEM CEF_BINARY_FILES libvulkan.so.1)
    ADD_LOGICAL_TARGET(libcef_lib "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")
endif()