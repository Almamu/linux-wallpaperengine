# - try to find the Xrandr library
#
# Cache Variables: (probably not for direct use in your scripts)
#  Xrandr_INCLUDE_DIR
#  Xrandr_SOURCE_DIR
#  Xrandr_LIBRARY
#
# Non-cache variables you might use in your CMakeLists.txt:
#  Xrandr_FOUND
#  Xrandr_INCLUDE_DIRS
#  Xrandr_LIBRARIES
#
# Requires these CMake modules:
#  FindPackageHandleStandardArgs (known included with CMake >=2.6.2)
#
# Original Author:
# 2014 Kevin M. Godby <kevin@godby.org>
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

set(Xrandr_ROOT_DIR
        "${Xrandr_ROOT_DIR}"
        CACHE
        PATH
        "Directory to search for Xrandr")

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_LIBXRANDR xrandr)
endif()

find_library(Xrandr_LIBRARY
        NAMES
        Xrandr
        PATHS
        ${PC_LIBXRANDR_LIBRARY_DIRS}
        ${PC_LIBXRANDR_LIBDIR}
        HINTS
        "${Xrandr_ROOT_DIR}"
        PATH_SUFFIXES
        lib
        )

get_filename_component(_libdir "${Xrandr_LIBRARY}" PATH)

find_path(Xrandr_INCLUDE_DIR
        NAMES
        Xrandr.h
        PATHS
        ${PC_LIBXRANDR_INCLUDE_DIRS}
        ${PC_LIBXRANDR_INCLUDEDIR}
        HINTS
        "${_libdir}"
        "${_libdir}/.."
        "${Xrandr_ROOT_DIR}"
        PATH_SUFFIXES
        X11
        X11/extensions
        )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Xrandr
        DEFAULT_MSG
        Xrandr_LIBRARY
        Xrandr_INCLUDE_DIR
        )

if(Xrandr_FOUND)
    list(APPEND Xrandr_LIBRARIES ${Xrandr_LIBRARY})
    list(APPEND Xrandr_INCLUDE_DIRS ${Xrandr_INCLUDE_DIR})
    mark_as_advanced(Xrandr_ROOT_DIR)
endif()

mark_as_advanced(Xrandr_INCLUDE_DIR
        Xrandr_LIBRARY)

