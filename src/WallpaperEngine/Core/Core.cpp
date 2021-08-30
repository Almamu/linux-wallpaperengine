#include <irrlicht/irrlicht.h>
#include "Core.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Core::Types;

irr::core::vector3df Core::ato3vf(const char *str)
{
    irr::f32 x = irr::core::fast_atof (str, &str); while (*str == ' ') str ++;
    irr::f32 y = irr::core::fast_atof (str, &str); while (*str == ' ') str ++;
    irr::f32 z = irr::core::fast_atof (str, &str);

    return irr::core::vector3df (x, y, z);
}

irr::core::vector2df Core::ato2vf (const char *str)
{
    irr::f32 x = irr::core::fast_atof (str, &str); while (*str == ' ') str ++;
    irr::f32 y = irr::core::fast_atof (str, &str);

    return irr::core::vector2df (x, y);
}

irr::core::vector3df Core::ato3vf (const std::string& str)
{
    return Core::ato3vf (str.c_str ());
}

irr::core::vector2df Core::ato2vf (const std::string& str)
{
    return Core::ato2vf (str.c_str ());
}

irr::video::SColorf Core::atoSColorf (const char *str)
{
    irr::core::vector3df vector = Core::ato3vf (str);

    return irr::video::SColorf (
            vector.X,
            vector.Y,
            vector.Z
    );
}

irr::video::SColorf Core::atoSColorf (const std::string& str)
{
    return Core::atoSColorf (str.c_str ());
}

irr::video::SColor Core::atoSColor (const char *str)
{
    irr::f32 r = irr::core::strtoul10 (str, &str); while (*str == ' ') str ++;
    irr::f32 g = irr::core::strtoul10 (str, &str); while (*str == ' ') str ++;
    irr::f32 b = irr::core::strtoul10 (str, &str);

    return irr::video::SColor (255, r, g, b);
}

glm::vec3 Core::aToVector3 (const char* str)
{
    float x = strtof (str, const_cast <char**> (&str)); while (*str == ' ') str ++;
    float y = strtof (str, const_cast <char**> (&str)); while (*str == ' ') str ++;
    float z = strtof (str, const_cast <char**> (&str));

    return {x, y, z};
}

glm::vec2 Core::aToVector2 (const char* str)
{
    float x = strtof (str, const_cast <char**> (&str)); while (*str == ' ') str ++;
    float y = strtof (str, const_cast <char**> (&str));

    return {x, y};
}

glm::vec3 Core::aToVector3 (const std::string& str)
{
    return Core::aToVector3 (str.c_str ());
}

glm::vec2 Core::aToVector2 (const std::string& str)
{
    return Core::aToVector2 (str.c_str ());
}

irr::video::SColor Core::atoSColor (const std::string& str)
{
    return Core::atoSColor (str.c_str ());
}

FloatColor Core::aToColor (const char* str)
{
    float r = strtof (str, const_cast<char **>(&str)); while (*str == ' ') str ++;
    float g = strtof (str, const_cast<char **>(&str)); while (*str == ' ') str ++;
    float b = strtof (str, const_cast<char **>(&str));

    return {r, g, b, 1.0f};
}

FloatColor Core::aToColor (const std::string& str)
{
    return aToColor (str.c_str ());
}

nlohmann::json::iterator Core::jsonFindRequired (nlohmann::json& data, const char *key, const char *notFoundMsg)
{
    auto value = data.find (key);

    if (value == data.end ())
    {
        throw std::runtime_error (notFoundMsg);
    }

    return value;
}

template <typename T> T Core::jsonFindDefault (nlohmann::json& data, const char *key, T defaultValue)
{
    auto value = data.find (key);

    if (value == data.end ())
    {
        return defaultValue;
    }

    return *value;
}

template bool Core::jsonFindDefault (nlohmann::json& data, const char *key, bool defaultValue);
template std::string Core::jsonFindDefault (nlohmann::json& data, const char *key, std::string defaultValue);
template irr::s16 Core::jsonFindDefault (nlohmann::json& data, const char *key, irr::s16 defaultValue);
template irr::u16 Core::jsonFindDefault (nlohmann::json& data, const char *key, irr::u16 defaultValue);
template irr::s32 Core::jsonFindDefault (nlohmann::json& data, const char *key, irr::s32 defaultValue);
template irr::u32 Core::jsonFindDefault (nlohmann::json& data, const char *key, irr::u32 defaultValue);
template irr::s64 Core::jsonFindDefault (nlohmann::json& data, const char *key, irr::s64 defaultValue);
template irr::u64 Core::jsonFindDefault (nlohmann::json& data, const char *key, irr::u64 defaultValue);
template irr::f32 Core::jsonFindDefault (nlohmann::json& data, const char *key, irr::f32 defaultValue);
template irr::f64 Core::jsonFindDefault (nlohmann::json& data, const char *key, irr::f64 defaultValue);