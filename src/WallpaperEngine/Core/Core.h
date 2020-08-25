#pragma once

#include <string>
#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>

namespace WallpaperEngine::Core
{
    irr::core::vector3df ato3vf (const char *str);
    irr::core::vector2df ato2vf (const char *str);

    irr::core::vector3df ato3vf (const std::string& str);
    irr::core::vector2df ato2vf (const std::string& str);

    irr::video::SColorf atoSColorf (const char *str);
    irr::video::SColorf atoSColorf (const std::string& str);

    irr::video::SColor atoSColor (const char *str);
    irr::video::SColor atoSColor (const std::string& str);

    nlohmann::json::iterator jsonFindRequired (nlohmann::json& data, const char *key, const char *notFoundMsg);
    template <typename T> T jsonFindDefault (nlohmann::json& data, const char *key, T defaultValue);
};
