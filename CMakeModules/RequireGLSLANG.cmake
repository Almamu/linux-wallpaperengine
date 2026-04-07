
option(ENABLE_OPT OFF)

if(NOT TARGET glslang)
    # glslang build
    option(BUILD_SHARED_LIBS OFF)
    option(ENABLE_GLSLANG_BINARIES OFF)
    option(GLSLANG_TESTS OFF)
    option(GLSLANG_ENABLE_INSTALL OFF)
    option(ENABLE_GLSLANG_JS OFF)
    option(ENABLE_HLSL OFF)

    add_subdirectory(${EXTERNAL_DEPS_DIR}/glslang-WallpaperEngine glslang)
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

    add_subdirectory(${EXTERNAL_DEPS_DIR}/SPIRV-Cross-WallpaperEngine spirv-cross)
endif()
