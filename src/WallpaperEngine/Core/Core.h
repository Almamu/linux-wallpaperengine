#pragma once

#include <string>
#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>

#include "WallpaperEngine/Core/Types/FloatColor.h"

namespace WallpaperEngine::Core
{
    using namespace WallpaperEngine::Core::Types;

    irr::core::vector3df ato3vf (const char *str);
    irr::core::vector2df ato2vf (const char *str);

    irr::core::vector3df ato3vf (const std::string& str);
    irr::core::vector2df ato2vf (const std::string& str);

    irr::video::SColorf atoSColorf (const char *str);
    irr::video::SColorf atoSColorf (const std::string& str);

    irr::video::SColor atoSColor (const char *str);
    irr::video::SColor atoSColor (const std::string& str);

    glm::vec3 aToVector3 (const char* str);
    glm::vec2 aToVector2 (const char* str);

    glm::vec3 aToVector3 (const std::string& str);
    glm::vec2 aToVector2 (const std::string& str);

    FloatColor aToColor (const char* str);
    FloatColor aToColor (const std::string& str);

    nlohmann::json::iterator jsonFindRequired (nlohmann::json& data, const char *key, const char *notFoundMsg);
    template <typename T> T jsonFindDefault (nlohmann::json& data, const char *key, T defaultValue);
};
