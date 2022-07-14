#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>

#include "WallpaperEngine/Core/Types/FloatColor.h"
#include "WallpaperEngine/Core/Types/IntegerColor.h"

namespace WallpaperEngine::Core
{
    using namespace WallpaperEngine::Core::Types;

    glm::vec4 aToVector4 (const char* str);
    glm::vec3 aToVector3 (const char* str);
    glm::vec2 aToVector2 (const char* str);

    glm::vec4 aToVector4 (const std::string& str);
    glm::vec3 aToVector3 (const std::string& str);
    glm::vec2 aToVector2 (const std::string& str);

    FloatColor aToColorf (const char* str);
    FloatColor aToColorf (const std::string& str);

    IntegerColor aToColori (const char* str);
    IntegerColor aToColori (const std::string& str);

    nlohmann::json::iterator jsonFindRequired (nlohmann::json& data, const char *key, const char *notFoundMsg);
    nlohmann::json::iterator jsonFindRequired (nlohmann::json::iterator& data, const char *key, const char *notFoundMsg);
    template <typename T> T jsonFindDefault (nlohmann::json& data, const char *key, T defaultValue);
};
