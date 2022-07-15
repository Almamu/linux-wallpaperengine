#include "Core.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Core::Types;

glm::vec4 Core::aToVector4 (const char* str)
{
    float x = strtof (str, const_cast <char**> (&str)); while (*str == ' ') str ++;
    float y = strtof (str, const_cast <char**> (&str)); while (*str == ' ') str ++;
    float z = strtof (str, const_cast <char**> (&str)); while (*str == ' ') str ++;
    float w = strtof (str, const_cast <char**> (&str));

    return {x, y, z, w};
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

glm::vec4 Core::aToVector4 (const std::string& str)
{
    return Core::aToVector4 (str.c_str ());
}

glm::vec3 Core::aToVector3 (const std::string& str)
{
    return Core::aToVector3 (str.c_str ());
}

glm::vec2 Core::aToVector2 (const std::string& str)
{
    return Core::aToVector2 (str.c_str ());
}

FloatColor Core::aToColorf (const char* str)
{
    float r = strtof (str, const_cast<char **>(&str)); while (*str == ' ') str ++;
    float g = strtof (str, const_cast<char **>(&str)); while (*str == ' ') str ++;
    float b = strtof (str, const_cast<char **>(&str));

    return {r, g, b, 1.0f};
}

FloatColor Core::aToColorf (const std::string& str)
{
    return aToColorf(str.c_str());
}

IntegerColor Core::aToColori (const char* str)
{
    uint8_t r = static_cast <uint8_t> (strtol (str, const_cast<char **>(&str), 10)); while (*str == ' ') str ++;
    uint8_t g = static_cast <uint8_t> (strtol (str, const_cast<char **>(&str), 10)); while (*str == ' ') str ++;
    uint8_t b = static_cast <uint8_t> (strtol (str, const_cast<char **>(&str), 10));

    return {r, g, b, 255};
}

IntegerColor Core::aToColori (const std::string& str)
{
    return aToColori(str.c_str());
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

nlohmann::json::iterator Core::jsonFindRequired (nlohmann::json::iterator& data, const char *key, const char *notFoundMsg)
{
    auto value = (*data).find (key);

    if (value == (*data).end ())
    {
        throw std::runtime_error (notFoundMsg);
    }

    return value;
}

template <typename T> T Core::jsonFindDefault (nlohmann::json& data, const char *key, T defaultValue)
{
    auto value = data.find (key);

    if (value == data.end () || value->type () == nlohmann::detail::value_t::null)
        return defaultValue;

    // type checks
    if ((std::is_same <T, float>::value || std::is_same <T, double>::value) && value->type () != nlohmann::detail::value_t::number_float)
    {
        fprintf(stderr, "%s is not of type double, returning default value\n", key);
        return defaultValue;
    }
    else if (std::is_same <T, std::string>::value && value->type () != nlohmann::detail::value_t::string)
    {
        fprintf (stderr, "%s is not of type string, returning default value\n", key);
        return defaultValue;
    }
    else if (std::is_same <T, bool>::value && value->type () != nlohmann::detail::value_t::boolean)
    {
        fprintf (stderr, "%s is not of type boolean, returning default value\n", key);
        return defaultValue;
    }

    // TODO: SUPPORT INTEGERS AND OTHER TYPES

    return *value;
}

template bool Core::jsonFindDefault (nlohmann::json& data, const char *key, bool defaultValue);
template std::string Core::jsonFindDefault (nlohmann::json& data, const char *key, std::string defaultValue);
template int16_t Core::jsonFindDefault (nlohmann::json& data, const char *key, int16_t defaultValue);
template uint16_t Core::jsonFindDefault (nlohmann::json& data, const char *key, uint16_t defaultValue);
template int32_t Core::jsonFindDefault (nlohmann::json& data, const char *key, int32_t defaultValue);
template uint32_t Core::jsonFindDefault (nlohmann::json& data, const char *key, uint32_t defaultValue);
template int64_t Core::jsonFindDefault (nlohmann::json& data, const char *key, int64_t defaultValue);
template uint64_t Core::jsonFindDefault (nlohmann::json& data, const char *key, uint64_t defaultValue);
template float Core::jsonFindDefault (nlohmann::json& data, const char *key, float defaultValue);
template double Core::jsonFindDefault (nlohmann::json& data, const char *key, double defaultValue);