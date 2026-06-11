if(NOT TARGET nlohmann_json::nlohmann_json)
    add_subdirectory(${EXTERNAL_DEPS_DIR}/json json)
endif()