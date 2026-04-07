# make sure to only create the targets once
# as multiple projects depend on it

option(ENABLE_OPT OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT TARGET glslang)
    # glslang build
    option(BUILD_SHARED_LIBS OFF)
    option(ENABLE_GLSLANG_BINARIES OFF)
    option(GLSLANG_TESTS OFF)
    option(GLSLANG_ENABLE_INSTALL OFF)
    option(ENABLE_GLSLANG_JS OFF)
    option(ENABLE_HLSL OFF)

    add_subdirectory(${CMAKE_SOURCE_DIR}/src/External/glslang-WallpaperEngine glslang)
endif()

if(NOT TARGET spirv-cross-core)
    # configure spirv to not output any files
    # just build as static library
    option(SPIRV_CROSS_FORCE_PIC ON)
    option(SPIRV_CROSS_CLI OFF)
    # need to force this because otherwise it wont work
    set(SPIRV_CROSS_SKIP_INSTALL ON CACHE BOOL "Skips installation targets." FORCE)
    option(SPIRV_CROSS_ENABLE_TESTS OFF)
    option(SPIRV_CROSS_ENABLE_HLSL OFF)
    option(SPIRV_CROSS_ENABLE_MSL OFF)
    option(SPIRV_CROSS_ENABLE_REFLECT OFF)
    option(SPIRV_CROSS_ENABLE_C_API OFF)
    option(SPIRV_CROSS_ENABLE_UTIL OFF)
    option(SPIRV_CROSS_ENABLE_REFLECT OFF)

    add_subdirectory(${CMAKE_SOURCE_DIR}/src/External/SPIRV-Cross-WallpaperEngine spirv-cross)
endif()

if(NOT TARGET kissfft)
    # configure kissfft build to static link and no files generation
    option(KISSFFT_TEST OFF)
    option(KISSFFT_TOOLS OFF)
    option(KISSFFT_PKGCONFIG OFF)
    # need to force this because otherwise it wont work
    set(KISSFFT_STATIC ON CACHE BOOL "Build kissfft as static (ON) or shared library (OFF)" FORCE)
    option(KISSFFT_ENABLE_INSTALL OFF)

    add_subdirectory(${CMAKE_SOURCE_DIR}/src/External/kissfft-WallpaperEngine kissfft)
endif()

if(NOT TARGET qjs)
    # quickjs build
    option(QJS_ENABLE_INSTALL OFF)

    add_subdirectory(${CMAKE_SOURCE_DIR}/src/External/quickjs qjs)
endif()