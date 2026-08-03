if(NOT TARGET Catch2::Catch2WithMain)
    add_subdirectory(${EXTERNAL_DEPS_DIR}/Catch2 catch2)
endif()
