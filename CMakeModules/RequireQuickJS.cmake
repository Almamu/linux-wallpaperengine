if(NOT TARGET qjs)
    # quickjs build
    option(QJS_ENABLE_INSTALL OFF)

    add_subdirectory(${EXTERNAL_DEPS_DIR}/quickjs qjs)
endif()