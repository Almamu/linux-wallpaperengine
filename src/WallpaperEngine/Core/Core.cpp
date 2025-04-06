#include "Core.h"

#include "WallpaperEngine/Core/UserSettings/CUserSettingBoolean.h"
#include "WallpaperEngine/Core/UserSettings/CUserSettingFloat.h"
#include "WallpaperEngine/Core/UserSettings/CUserSettingVector3.h"
#include "WallpaperEngine/Logging/CLog.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Core::UserSettings;

glm::vec4 Core::aToVector4 (const char* str) {
    float x = strtof (str, const_cast<char**> (&str));
    while (*str == ' ')
        str++;
    float y = strtof (str, const_cast<char**> (&str));
    while (*str == ' ')
        str++;
    float z = strtof (str, const_cast<char**> (&str));
    while (*str == ' ')
        str++;
    float w = strtof (str, const_cast<char**> (&str));

    return {x, y, z, w};
}

glm::vec3 Core::aToVector3 (const char* str) {
    float x = strtof (str, const_cast<char**> (&str));
    while (*str == ' ')
        str++;
    float y = strtof (str, const_cast<char**> (&str));
    while (*str == ' ')
        str++;
    float z = strtof (str, const_cast<char**> (&str));

    return {x, y, z};
}

glm::vec2 Core::aToVector2 (const char* str) {
    float x = strtof (str, const_cast<char**> (&str));
    while (*str == ' ')
        str++;
    float y = strtof (str, const_cast<char**> (&str));

    return {x, y};
}

glm::vec4 Core::aToVector4 (const std::string& str) {
    return Core::aToVector4 (str.c_str ());
}

glm::vec3 Core::aToVector3 (const std::string& str) {
    return Core::aToVector3 (str.c_str ());
}

glm::vec2 Core::aToVector2 (const std::string& str) {
    return Core::aToVector2 (str.c_str ());
}

glm::vec3 Core::aToColorf (const char* str) {
    float r = strtof (str, const_cast<char**> (&str));
    while (*str == ' ')
        str++;
    float g = strtof (str, const_cast<char**> (&str));
    while (*str == ' ')
        str++;
    float b = strtof (str, const_cast<char**> (&str));

    return {r, g, b};
}

glm::vec3 Core::aToColorf (const std::string& str) {
    return aToColorf (str.c_str ());
}

glm::ivec3 Core::aToColori (const char* str) {
    auto r = static_cast<uint8_t> (strtol (str, const_cast<char**> (&str), 10));
    while (*str == ' ')
        str++;
    auto g = static_cast<uint8_t> (strtol (str, const_cast<char**> (&str), 10));
    while (*str == ' ')
        str++;
    auto b = static_cast<uint8_t> (strtol (str, const_cast<char**> (&str), 10));

    return {r, g, b};
}

glm::ivec3 Core::aToColori (const std::string& str) {
    return aToColori (str.c_str ());
}

nlohmann::json::iterator Core::jsonFindRequired (nlohmann::json& data, const char* key, const char* notFoundMsg) {
    auto value = data.find (key);

    if (value == data.end ())
        sLog.exception ("Cannot find required key (", key, ") in json: ", notFoundMsg);

    return value;
}

nlohmann::json::iterator Core::jsonFindRequired (const nlohmann::json::iterator& data, const char* key,
                                                 const char* notFoundMsg) {
    auto value = data->find (key);

    if (value == data->end ())
        sLog.exception ("Cannot find required key (", key, ") in json: ", notFoundMsg);

    return value;
}

template <typename T> T Core::jsonFindDefault (nlohmann::json& data, const char* key, T defaultValue) {
    const auto value = data.find (key);

    if (value == data.end () || value->type () == nlohmann::detail::value_t::null)
        return defaultValue;

    // type checks
    if ((std::is_same_v<T, float> || std::is_same_v<T, double>) ) {
        if (value->type () != nlohmann::detail::value_t::number_float &&
            value->type () != nlohmann::detail::value_t::number_integer &&
            value->type () != nlohmann::detail::value_t::number_unsigned) {
            sLog.error (key, " is not of type double or integer, returning default value");
            return defaultValue;
        }
    } else if (std::is_same_v<T, std::string> && value->type () != nlohmann::detail::value_t::string) {
        sLog.error (key, " is not of type string, returning default value");
        return defaultValue;
    } else if (std::is_same_v<T, bool> && value->type () != nlohmann::detail::value_t::boolean) {
        sLog.error (key, " is not of type boolean, returning default value");
        return defaultValue;
    }

    // TODO: SUPPORT INTEGERS AND OTHER TYPES

    return *value;
}

template bool Core::jsonFindDefault (nlohmann::json& data, const char* key, bool defaultValue);
template std::string Core::jsonFindDefault (nlohmann::json& data, const char* key, std::string defaultValue);
template int16_t Core::jsonFindDefault (nlohmann::json& data, const char* key, int16_t defaultValue);
template uint16_t Core::jsonFindDefault (nlohmann::json& data, const char* key, uint16_t defaultValue);
template int32_t Core::jsonFindDefault (nlohmann::json& data, const char* key, int32_t defaultValue);
template uint32_t Core::jsonFindDefault (nlohmann::json& data, const char* key, uint32_t defaultValue);
template int64_t Core::jsonFindDefault (nlohmann::json& data, const char* key, int64_t defaultValue);
template uint64_t Core::jsonFindDefault (nlohmann::json& data, const char* key, uint64_t defaultValue);
template float Core::jsonFindDefault (nlohmann::json& data, const char* key, float defaultValue);
template double Core::jsonFindDefault (nlohmann::json& data, const char* key, double defaultValue);

template <typename T>
T* Core::jsonFindUserConfig (nlohmann::json& data, const char* key, typename T::data_type defaultValue) {
    const auto it = data.find (key);

    if (it == data.end () || it->type () == nlohmann::detail::value_t::null)
        return T::fromScalar (defaultValue);

    return T::fromJSON (*it);
}

template CUserSettingBoolean* Core::jsonFindUserConfig (nlohmann::json& data, const char* key,
                                                        CUserSettingBoolean::data_type defaultValue);
template CUserSettingVector3* Core::jsonFindUserConfig (nlohmann::json& data, const char* key,
                                                        CUserSettingVector3::data_type defaultValue);
template CUserSettingFloat* Core::jsonFindUserConfig (nlohmann::json& data, const char* key,
                                                      CUserSettingFloat::data_type defaultValue);