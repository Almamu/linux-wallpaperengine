# Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

# Download the CEF binary distribution for |platform| and |version| to
# |download_dir|. The |CEF_ROOT| variable will be set in global scope pointing
# to the extracted location.
# Visit https://cef-builds.spotifycdn.com/index.html for the list of
# supported platforms and versions.

function(DownloadCEF platform version download_dir)
  # Specify the binary distribution type and download directory.
  set(CEF_DISTRIBUTION "cef_binary_${version}_${platform}")
  set(CEF_DOWNLOAD_DIR "${download_dir}")

  # The location where we expect the extracted binary distribution.
  set(CEF_ROOT "${CEF_DOWNLOAD_DIR}/${CEF_DISTRIBUTION}" CACHE INTERNAL "CEF_ROOT")

  # Download and/or extract the binary distribution if necessary.
  if(NOT IS_DIRECTORY "${CEF_ROOT}")
    set(CEF_DOWNLOAD_FILENAME "${CEF_DISTRIBUTION}.tar.bz2")
    set(CEF_DOWNLOAD_PATH "${CEF_DOWNLOAD_DIR}/${CEF_DOWNLOAD_FILENAME}")
    if(NOT EXISTS "${CEF_DOWNLOAD_PATH}")
      set(CEF_DOWNLOAD_URL "https://cef-builds.spotifycdn.com/${CEF_DOWNLOAD_FILENAME}")
      string(REPLACE "+" "%2B" CEF_DOWNLOAD_URL_ESCAPED ${CEF_DOWNLOAD_URL})

      # Download the SHA1 hash for the binary distribution.
      message(STATUS "Downloading ${CEF_DOWNLOAD_PATH}.sha1 from ${CEF_DOWNLOAD_URL_ESCAPED}...")
      file(DOWNLOAD "${CEF_DOWNLOAD_URL_ESCAPED}.sha1" "${CEF_DOWNLOAD_PATH}.sha1")
      file(READ "${CEF_DOWNLOAD_PATH}.sha1" CEF_SHA1)

      # Download the binary distribution and verify the hash.
      message(STATUS "Downloading ${CEF_DOWNLOAD_PATH}...")
      file(
        DOWNLOAD "${CEF_DOWNLOAD_URL_ESCAPED}" "${CEF_DOWNLOAD_PATH}"
        EXPECTED_HASH SHA1=${CEF_SHA1}
        SHOW_PROGRESS
        )
    endif()

    # Extract the binary distribution.
    message(STATUS "Extracting ${CEF_DOWNLOAD_PATH}...")
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E tar xzf "${CEF_DOWNLOAD_DIR}/${CEF_DOWNLOAD_FILENAME}"
      WORKING_DIRECTORY ${CEF_DOWNLOAD_DIR}
      )
  endif()
endfunction()


# Set common target properties. Use SET_LIBRARY_TARGET_PROPERTIES() or
# SET_EXECUTABLE_TARGET_PROPERTIES() instead of calling this macro directly.
macro(REPLACED_SET_COMMON_TARGET_PROPERTIES target)
  # Compile flags. (MODIFIED FOR C/C++ SEPARATION)
  target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:C>:${CEF_COMPILER_FLAGS}>)
  target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${CEF_CXX_COMPILER_FLAGS}>)
  target_compile_options(${target} PRIVATE $<$<CONFIG:Debug>:${CEF_COMPILER_FLAGS_DEBUG} ${CEF_CXX_COMPILER_FLAGS_DEBUG}>)
  target_compile_options(${target} PRIVATE $<$<CONFIG:Release>:${CEF_COMPILER_FLAGS_RELEASE} ${CEF_CXX_COMPILER_FLAGS_RELEASE}>)

  # Compile definitions.
  target_compile_definitions(${target} PRIVATE ${CEF_COMPILER_DEFINES})
  target_compile_definitions(${target} PRIVATE $<$<CONFIG:Debug>:${CEF_COMPILER_DEFINES_DEBUG}>)
  target_compile_definitions(${target} PRIVATE $<$<CONFIG:Release>:${CEF_COMPILER_DEFINES_RELEASE}>)

  # Include directories.
  target_include_directories(${target} PRIVATE ${CEF_INCLUDE_PATH})

  # Linker flags.
  if(CEF_LINKER_FLAGS)
    string(REPLACE ";" " " _flags_str "${CEF_LINKER_FLAGS}")
    set_property(TARGET ${target} PROPERTY LINK_FLAGS ${_flags_str})
  endif()
  if(CEF_LINKER_FLAGS_DEBUG)
    string(REPLACE ";" " " _flags_str "${CEF_LINKER_FLAGS_DEBUG}")
    set_property(TARGET ${target} PROPERTY LINK_FLAGS_DEBUG ${_flags_str})
  endif()
  if(CEF_LINKER_FLAGS_RELEASE)
    string(REPLACE ";" " " _flags_str "${CEF_LINKER_FLAGS_RELEASE}")
    set_property(TARGET ${target} PROPERTY LINK_FLAGS_RELEASE ${_flags_str})
  endif()

  if(OS_MAC)
    # Set Xcode target properties.
    set_target_properties(${target} PROPERTIES
            XCODE_ATTRIBUTE_ALWAYS_SEARCH_USER_PATHS                    NO
            XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD                 "gnu++11"   # -std=gnu++11
            XCODE_ATTRIBUTE_CLANG_LINK_OBJC_RUNTIME                     NO          # -fno-objc-link-runtime
            XCODE_ATTRIBUTE_CLANG_WARN_OBJC_MISSING_PROPERTY_SYNTHESIS  YES         # -Wobjc-missing-property-synthesis
            XCODE_ATTRIBUTE_COPY_PHASE_STRIP                            NO
            XCODE_ATTRIBUTE_DEAD_CODE_STRIPPING[variant=Release]        YES         # -Wl,-dead_strip
            XCODE_ATTRIBUTE_GCC_C_LANGUAGE_STANDARD                     "c99"       # -std=c99
            XCODE_ATTRIBUTE_GCC_CW_ASM_SYNTAX                           NO          # No -fasm-blocks
            XCODE_ATTRIBUTE_GCC_DYNAMIC_NO_PIC                          NO
            XCODE_ATTRIBUTE_GCC_ENABLE_CPP_EXCEPTIONS                   NO          # -fno-exceptions
            XCODE_ATTRIBUTE_GCC_ENABLE_CPP_RTTI                         NO          # -fno-rtti
            XCODE_ATTRIBUTE_GCC_ENABLE_PASCAL_STRINGS                   NO          # No -mpascal-strings
            XCODE_ATTRIBUTE_GCC_INLINES_ARE_PRIVATE_EXTERN              YES         # -fvisibility-inlines-hidden
            XCODE_ATTRIBUTE_GCC_OBJC_CALL_CXX_CDTORS                    YES         # -fobjc-call-cxx-cdtors
            XCODE_ATTRIBUTE_GCC_SYMBOLS_PRIVATE_EXTERN                  YES         # -fvisibility=hidden
            XCODE_ATTRIBUTE_GCC_THREADSAFE_STATICS                      NO          # -fno-threadsafe-statics
            XCODE_ATTRIBUTE_GCC_TREAT_WARNINGS_AS_ERRORS                YES         # -Werror
            XCODE_ATTRIBUTE_GCC_VERSION                                 "com.apple.compilers.llvm.clang.1_0"
            XCODE_ATTRIBUTE_GCC_WARN_ABOUT_MISSING_NEWLINE              YES         # -Wnewline-eof
            XCODE_ATTRIBUTE_USE_HEADERMAP                               NO
            OSX_ARCHITECTURES_DEBUG                                     "${CMAKE_OSX_ARCHITECTURES}"
            OSX_ARCHITECTURES_RELEASE                                   "${CMAKE_OSX_ARCHITECTURES}"
    )
  endif()
endmacro()

# Set executable-specific properties.
macro(REPLACED_SET_EXECUTABLE_TARGET_PROPERTIES target)
  REPLACED_SET_COMMON_TARGET_PROPERTIES(${target})

  # Executable linker flags.
  if(CEF_EXE_LINKER_FLAGS)
    string(REPLACE ";" " " _flags_str "${CEF_EXE_LINKER_FLAGS}")
    set_property(TARGET ${target} PROPERTY LINK_FLAGS ${_flags_str})
  endif()
  if(CEF_EXE_LINKER_FLAGS_DEBUG)
    string(REPLACE ";" " " _flags_str "${CEF_EXE_LINKER_FLAGS_DEBUG}")
    set_property(TARGET ${target} PROPERTY LINK_FLAGS_DEBUG ${_flags_str})
  endif()
  if(CEF_EXE_LINKER_FLAGS_RELEASE)
    string(REPLACE ";" " " _flags_str "${CEF_EXE_LINKER_FLAGS_RELEASE}")
    set_property(TARGET ${target} PROPERTY LINK_FLAGS_RELEASE ${_flags_str})
  endif()
endmacro()
