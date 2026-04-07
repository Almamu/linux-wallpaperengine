if(NOT TARGET kissfft)
    # configure kissfft build to static link and no files generation
    option(KISSFFT_TEST OFF)
    option(KISSFFT_TOOLS OFF)
    option(KISSFFT_PKGCONFIG OFF)
    set(KISSFFT_STATIC ON CACHE BOOL "Build kissfft as static (ON) or shared library (OFF)" FORCE)
    option(KISSFFT_ENABLE_INSTALL OFF)

    add_subdirectory(${EXTERNAL_DEPS_DIR}/kissfft-WallpaperEngine kissfft)
endif()
