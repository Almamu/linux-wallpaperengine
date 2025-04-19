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

glm::ivec4 Core::aToVector4i (const char* str) {
    int x = strtol (str, const_cast<char**> (&str), 10);
    while (*str == ' ')
        str++;
    int y = strtol (str, const_cast<char**> (&str), 10);
    while (*str == ' ')
        str++;
    int z = strtol (str, const_cast<char**> (&str), 10);
    while (*str == ' ')
        str++;
    int w = strtol (str, const_cast<char**> (&str), 10);

    return {x, y, z, w};
}

glm::ivec3 Core::aToVector3i (const char* str) {
    int x = strtol (str, const_cast<char**> (&str), 10);
    while (*str == ' ')
        str++;
    int y = strtol (str, const_cast<char**> (&str), 10);
    while (*str == ' ')
        str++;
    int z = strtol (str, const_cast<char**> (&str), 10);

    return {x, y, z};
}

glm::ivec2 Core::aToVector2i (const char* str) {
    int x = strtol (str, const_cast<char**> (&str), 10);
    while (*str == ' ')
        str++;
    int y = strtol (str, const_cast<char**> (&str), 10);

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

glm::ivec4 Core::aToVector4i (const std::string& str) {
    return Core::aToVector4i (str.c_str ());
}

glm::ivec3 Core::aToVector3i (const std::string& str) {
    return Core::aToVector3i (str.c_str ());
}

glm::ivec2 Core::aToVector2i (const std::string& str) {
    return Core::aToVector2i (str.c_str ());
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

template <typename T> bool typeCheck (const nlohmann::json::const_iterator& value) {
    if (value->type () == nlohmann::detail::value_t::null) {
        return false;
    }

    // type checks
    if constexpr ((std::is_same_v<T, float> || std::is_same_v<T, double>) ) {
        if (value->type () != nlohmann::detail::value_t::number_float &&
            value->type () != nlohmann::detail::value_t::number_integer &&
            value->type () != nlohmann::detail::value_t::number_unsigned) {
            return false;
        }
    } else if constexpr (std::is_same_v<T, std::string>) {
        if (value->type () != nlohmann::detail::value_t::string) {
            return false;
        }
    } else if constexpr (std::is_same_v<T, bool>) {
        if (value->type () != nlohmann::detail::value_t::boolean) {
            return false;
        }
    }

    return true;
}

template <typename T> const T Core::jsonFindRequired (
    const nlohmann::json::const_iterator& data, const char* key, const char* notFoundMsg
) {
    const auto iterator = jsonFindRequired (data, key, notFoundMsg);

    // vector types need of special handling
    if constexpr (std::is_same_v<T, std::string>) {
        // std::strings are special, type checking doesn't need to happen, we just want the string representation
        // of whatever is in there
        if (iterator->is_number_integer ()) {
            return std::to_string (iterator->get <int> ());
        } else if (iterator->is_number_float ()) {
            return std::to_string (iterator->get <float> ());
        } else if (iterator->is_boolean()) {
            return std::to_string (iterator->get <bool> ());
        } else if (iterator->is_null ()) {
            return "null";
        } else if (iterator->is_string ()) {
            return *iterator;
        }
    } else if  constexpr (std::is_same_v<T, glm::vec4>) {
        if (!typeCheck<T> (iterator)) {
            sLog.exception ("key value doesn't match expected type. Got ", iterator->type_name(), " expected vector-like-string", ": ", notFoundMsg);
        }

        return aToVector4 (*iterator);
    } else if constexpr (std::is_same_v<T, glm::vec3>) {
        if (!typeCheck<T> (iterator)) {
            sLog.exception ("key value doesn't match expected type. Got ", iterator->type_name(), " expected vector-like-string", ": ", notFoundMsg);
        }

        return aToVector3 (*iterator);
    } else if constexpr (std::is_same_v<T, glm::vec2>) {
        if (!typeCheck<T> (iterator)) {
            sLog.exception ("key value doesn't match expected type. Got ", iterator->type_name(), " expected vector-like-string", ": ", notFoundMsg);
        }

        return aToVector2 (*iterator);
    } else if constexpr (std::is_same_v<T, glm::ivec4>) {
        if (!typeCheck<T> (iterator)) {
            sLog.exception ("key value doesn't match expected type. Got ", iterator->type_name(), " expected vector-like-string", ": ", notFoundMsg);
        }

        return aToVector4i (*iterator);
    } else if constexpr (std::is_same_v<T, glm::ivec3>) {
        if (!typeCheck<T> (iterator)) {
            sLog.exception ("key value doesn't match expected type. Got ", iterator->type_name(), " expected vector-like-string", ": ", notFoundMsg);
        }

        return aToVector3i (*iterator);
    } else if constexpr (std::is_same_v<T, glm::ivec2>) {
        if (!typeCheck<T> (iterator)) {
            sLog.exception ("key value doesn't match expected type. Got ", iterator->type_name(), " expected vector-like-string", ": ", notFoundMsg);
        }

        return aToVector2i (*iterator);
    } else if (typeCheck<T> (iterator)) {
        return *iterator;
    }

    sLog.exception ("key value doesn't match expected type. Got ", iterator->type_name(), ": ", notFoundMsg);
}

template const bool Core::jsonFindRequired (const nlohmann::json::const_iterator& data, const char* key, const char* notFoundMsg);
template const std::string Core::jsonFindRequired (const nlohmann::json::const_iterator& data, const char* key, const char* notFoundMsg);
template const int16_t Core::jsonFindRequired (const nlohmann::json::const_iterator& data, const char* key, const char* notFoundMsg);
template const uint16_t Core::jsonFindRequired (const nlohmann::json::const_iterator& data, const char* key, const char* notFoundMsg);
template const int32_t Core::jsonFindRequired (const nlohmann::json::const_iterator& data, const char* key, const char* notFoundMsg);
template const uint32_t Core::jsonFindRequired (const nlohmann::json::const_iterator& data, const char* key, const char* notFoundMsg);
template const int64_t Core::jsonFindRequired (const nlohmann::json::const_iterator& data, const char* key, const char* notFoundMsg);
template const uint64_t Core::jsonFindRequired (const nlohmann::json::const_iterator& data, const char* key, const char* notFoundMsg);
template const float Core::jsonFindRequired (const nlohmann::json::const_iterator& data, const char* key, const char* notFoundMsg);
template const double Core::jsonFindRequired (const nlohmann::json::const_iterator& data, const char* key, const char* notFoundMsg);
template const glm::vec4 Core::jsonFindRequired (const nlohmann::json::const_iterator& data, const char* key, const char* notFoundMsg);
template const glm::vec3 Core::jsonFindRequired (const nlohmann::json::const_iterator& data, const char* key, const char* notFoundMsg);
template const glm::vec2 Core::jsonFindRequired (const nlohmann::json::const_iterator& data, const char* key, const char* notFoundMsg);
template const glm::ivec4 Core::jsonFindRequired (const nlohmann::json::const_iterator& data, const char* key, const char* notFoundMsg);
template const glm::ivec3 Core::jsonFindRequired (const nlohmann::json::const_iterator& data, const char* key, const char* notFoundMsg);
template const glm::ivec2 Core::jsonFindRequired (const nlohmann::json::const_iterator& data, const char* key, const char* notFoundMsg);

template <typename T> const T Core::jsonFindRequired (
    const nlohmann::json& data, const char* key, const char* notFoundMsg
) {
    const auto iterator = jsonFindRequired (data, key, notFoundMsg);

    // vector types need of special handling
    if constexpr (std::is_same_v<T, std::string>) {
        // std::strings are special, type checking doesn't need to happen, we just want the string representation
        // of whatever is in there
        if (iterator->is_number_integer ()) {
            return std::to_string (iterator->get <int> ());
        } else if (iterator->is_number_float ()) {
            return std::to_string (iterator->get <float> ());
        } else if (iterator->is_boolean()) {
            return std::to_string (iterator->get <bool> ());
        } else if (iterator->is_null ()) {
            return "null";
        } else if (iterator->is_string ()) {
            return *iterator;
        }
    } else if constexpr (std::is_same_v<T, glm::vec4>) {
        if (!typeCheck<T> (iterator)) {
            sLog.exception ("key value doesn't match expected type. Got ", iterator->type_name(), " expected vector-like-string", ": ", notFoundMsg);
        }

        return aToVector4 (*iterator);
    } else if constexpr (std::is_same_v<T, glm::vec3>) {
        if (!typeCheck<T> (iterator)) {
            sLog.exception ("key value doesn't match expected type. Got ", iterator->type_name(), " expected vector-like-string", ": ", notFoundMsg);
        }

        return aToVector3 (*iterator);
    } else if constexpr (std::is_same_v<T, glm::vec2>) {
        if (!typeCheck<T> (iterator)) {
            sLog.exception ("key value doesn't match expected type. Got ", iterator->type_name(), " expected vector-like-string", ": ", notFoundMsg);
        }

        return aToVector2 (*iterator);
    } else if constexpr (std::is_same_v<T, glm::ivec4>) {
        if (!typeCheck<T> (iterator)) {
            sLog.exception ("key value doesn't match expected type. Got ", iterator->type_name(), " expected vector-like-string", ": ", notFoundMsg);
        }

        return aToVector4i (*iterator);
    } else if constexpr (std::is_same_v<T, glm::ivec3>) {
        if (!typeCheck<T> (iterator)) {
            sLog.exception ("key value doesn't match expected type. Got ", iterator->type_name(), " expected vector-like-string", ": ", notFoundMsg);
        }

        return aToVector3i (*iterator);
    } else if constexpr (std::is_same_v<T, glm::ivec2>) {
        if (!typeCheck<T> (iterator)) {
            sLog.exception ("key value doesn't match expected type. Got ", iterator->type_name(), " expected vector-like-string", ": ", notFoundMsg);
        }

        return aToVector2i (*iterator);
    } else if (typeCheck<T> (iterator)) {
        return *iterator;
    }

    sLog.exception ("key value doesn't match expected type. Got ", iterator->type_name(), ": ", notFoundMsg);
}

template const bool Core::jsonFindRequired (const nlohmann::json& data, const char* key, const char* notFoundMsg);
template const std::string Core::jsonFindRequired (const nlohmann::json& data, const char* key, const char* notFoundMsg);
template const int16_t Core::jsonFindRequired (const nlohmann::json& data, const char* key, const char* notFoundMsg);
template const uint16_t Core::jsonFindRequired (const nlohmann::json& data, const char* key, const char* notFoundMsg);
template const int32_t Core::jsonFindRequired (const nlohmann::json& data, const char* key, const char* notFoundMsg);
template const uint32_t Core::jsonFindRequired (const nlohmann::json& data, const char* key, const char* notFoundMsg);
template const int64_t Core::jsonFindRequired (const nlohmann::json& data, const char* key, const char* notFoundMsg);
template const uint64_t Core::jsonFindRequired (const nlohmann::json& data, const char* key, const char* notFoundMsg);
template const float Core::jsonFindRequired (const nlohmann::json& data, const char* key, const char* notFoundMsg);
template const double Core::jsonFindRequired (const nlohmann::json& data, const char* key, const char* notFoundMsg);
template const glm::vec4 Core::jsonFindRequired (const nlohmann::json& data, const char* key, const char* notFoundMsg);
template const glm::vec3 Core::jsonFindRequired (const nlohmann::json& data, const char* key, const char* notFoundMsg);
template const glm::vec2 Core::jsonFindRequired (const nlohmann::json& data, const char* key, const char* notFoundMsg);
template const glm::ivec4 Core::jsonFindRequired (const nlohmann::json& data, const char* key, const char* notFoundMsg);
template const glm::ivec3 Core::jsonFindRequired (const nlohmann::json& data, const char* key, const char* notFoundMsg);
template const glm::ivec2 Core::jsonFindRequired (const nlohmann::json& data, const char* key, const char* notFoundMsg);

nlohmann::json::const_iterator Core::jsonFindRequired (
    const nlohmann::json::const_iterator& data, const char* key, const char* notFoundMsg
) {
    auto value = data->find (key);

    if (value == data->end ())
        sLog.exception ("Cannot find required key (", key, ") in json: ", notFoundMsg);

    return value;
}

nlohmann::json::const_iterator Core::jsonFindRequired (
    const nlohmann::json& data, const char* key, const char* notFoundMsg
) {
    auto value = data.find (key);

    if (value == data.end ())
        sLog.exception ("Cannot find required key (", key, ") in json: ", notFoundMsg);

    return value;
}

template <typename T> const T Core::jsonFindDefault (
    const nlohmann::json::const_iterator& data, const char* key, const T defaultValue
) {
    const auto value = data->find (key);

    if (value == data->end () || value->type () == nlohmann::detail::value_t::null)
        return defaultValue;

    // vector types need of special handling
    if constexpr (std::is_same_v<T, std::string>) {
        // std::strings are special, type checking doesn't need to happen, we just want the string representation
        // of whatever is in there
        if (value->is_number_integer ()) {
            return std::to_string (value->get <int> ());
        } else if (value->is_number_float ()) {
            return std::to_string (value->get <float> ());
        } else if (value->is_boolean()) {
            return std::to_string (value->get <bool> ());
        } else if (value->is_null ()) {
            return "null";
        } else if (value->is_string ()) {
            return *value;
        }
    } else if constexpr (std::is_same_v<T, glm::vec4>) {
        if (!typeCheck<T> (value)) {
            return defaultValue;
        }

        return aToVector4 (*value);
    } else if constexpr (std::is_same_v<T, glm::vec3>) {
        if (!typeCheck<T> (value)) {
            return defaultValue;
        }

        return aToVector3 (*value);
    } else if constexpr (std::is_same_v<T, glm::vec2>) {
        if (!typeCheck<T> (value)) {
            return defaultValue;
        }

        return aToVector2 (*value);
    } else if constexpr (std::is_same_v<T, glm::ivec4>) {
        if (!typeCheck<T> (value)) {
            return defaultValue;
        }

        return aToVector4i (*value);
    } else if constexpr (std::is_same_v<T, glm::ivec3>) {
        if (!typeCheck<T> (value)) {
            return defaultValue;
        }

        return aToVector3i (*value);
    } else if constexpr (std::is_same_v<T, glm::ivec2>) {
        if (!typeCheck<T> (value)) {
            return defaultValue;
        }

        return aToVector2i (*value);
    } else if (typeCheck<T> (value)) {
        return *value;
    }

    return defaultValue;
}

template const bool Core::jsonFindDefault (const nlohmann::json::const_iterator& data, const char* key, const bool defaultValue);
template const std::string Core::jsonFindDefault (const nlohmann::json::const_iterator& data, const char* key, const std::string defaultValue);
template const int16_t Core::jsonFindDefault (const nlohmann::json::const_iterator& data, const char* key, const int16_t defaultValue);
template const uint16_t Core::jsonFindDefault (const nlohmann::json::const_iterator& data, const char* key, const uint16_t defaultValue);
template const int32_t Core::jsonFindDefault (const nlohmann::json::const_iterator& data, const char* key, const int32_t defaultValue);
template const uint32_t Core::jsonFindDefault (const nlohmann::json::const_iterator& data, const char* key, const uint32_t defaultValue);
template const int64_t Core::jsonFindDefault (const nlohmann::json::const_iterator& data, const char* key, const int64_t defaultValue);
template const uint64_t Core::jsonFindDefault (const nlohmann::json::const_iterator& data, const char* key, const uint64_t defaultValue);
template const float Core::jsonFindDefault (const nlohmann::json::const_iterator& data, const char* key, const float defaultValue);
template const double Core::jsonFindDefault (const nlohmann::json::const_iterator& data, const char* key, const double defaultValue);
template const glm::vec2 Core::jsonFindDefault (const nlohmann::json::const_iterator& data, const char* key, const glm::vec2 defaultValue);
template const glm::vec3 Core::jsonFindDefault (const nlohmann::json::const_iterator& data, const char* key, const glm::vec3 defaultValue);
template const glm::vec4 Core::jsonFindDefault (const nlohmann::json::const_iterator& data, const char* key, const glm::vec4 defaultValue);
template const glm::ivec2 Core::jsonFindDefault (const nlohmann::json::const_iterator& data, const char* key, const glm::ivec2 defaultValue);
template const glm::ivec3 Core::jsonFindDefault (const nlohmann::json::const_iterator& data, const char* key, const glm::ivec3 defaultValue);
template const glm::ivec4 Core::jsonFindDefault (const nlohmann::json::const_iterator& data, const char* key, const glm::ivec4 defaultValue);

template <typename T> const T Core::jsonFindDefault (
    const nlohmann::json& data, const char* key, const T defaultValue
) {
    const auto value = data.find (key);

    if (value == data.end () || value->type () == nlohmann::detail::value_t::null)
        return defaultValue;

    // vector types need of special handling
    if constexpr (std::is_same_v<T, glm::vec4>) {
        if (!typeCheck<T> (value)) {
            return defaultValue;
        }

        return aToVector4 (*value);
    } else if constexpr (std::is_same_v<T, glm::vec3>) {
        if (!typeCheck<T> (value)) {
            return defaultValue;
        }

        return aToVector3 (*value);
    } else if constexpr (std::is_same_v<T, glm::vec2>) {
        if (!typeCheck<T> (value)) {
            return defaultValue;
        }

        return aToVector2 (*value);
    } else if constexpr (std::is_same_v<T, glm::ivec4>) {
        if (!typeCheck<T> (value)) {
            return defaultValue;
        }

        return aToVector4i (*value);
    } else if constexpr (std::is_same_v<T, glm::ivec3>) {
        if (!typeCheck<T> (value)) {
            return defaultValue;
        }

        return aToVector3i (*value);
    } else if constexpr (std::is_same_v<T, glm::ivec2>) {
        if (!typeCheck<T> (value)) {
            return defaultValue;
        }

        return aToVector2i (*value);
    } else if (typeCheck<T> (value)) {
        return *value;
    }

    return defaultValue;
}

template const bool Core::jsonFindDefault (const nlohmann::json& data, const char* key, const bool defaultValue);
template const std::string Core::jsonFindDefault (const nlohmann::json& data, const char* key, const std::string defaultValue);
template const int16_t Core::jsonFindDefault (const nlohmann::json& data, const char* key, const int16_t defaultValue);
template const uint16_t Core::jsonFindDefault (const nlohmann::json& data, const char* key, const uint16_t defaultValue);
template const int32_t Core::jsonFindDefault (const nlohmann::json& data, const char* key, const int32_t defaultValue);
template const uint32_t Core::jsonFindDefault (const nlohmann::json& data, const char* key, const uint32_t defaultValue);
template const int64_t Core::jsonFindDefault (const nlohmann::json& data, const char* key, const int64_t defaultValue);
template const uint64_t Core::jsonFindDefault (const nlohmann::json& data, const char* key, const uint64_t defaultValue);
template const float Core::jsonFindDefault (const nlohmann::json& data, const char* key, const float defaultValue);
template const double Core::jsonFindDefault (const nlohmann::json& data, const char* key, const double defaultValue);
template const glm::vec2 Core::jsonFindDefault (const nlohmann::json& data, const char* key, const glm::vec2 defaultValue);
template const glm::vec3 Core::jsonFindDefault (const nlohmann::json& data, const char* key, const glm::vec3 defaultValue);
template const glm::vec4 Core::jsonFindDefault (const nlohmann::json& data, const char* key, const glm::vec4 defaultValue);
template const glm::ivec2 Core::jsonFindDefault (const nlohmann::json& data, const char* key, const glm::ivec2 defaultValue);
template const glm::ivec3 Core::jsonFindDefault (const nlohmann::json& data, const char* key, const glm::ivec3 defaultValue);
template const glm::ivec4 Core::jsonFindDefault (const nlohmann::json& data, const char* key, const glm::ivec4 defaultValue);

template <typename T> const T* Core::jsonFindUserConfig (
    const nlohmann::json::const_iterator& data, const CProject& project, const char* key, typename T::data_type defaultValue
) {
    const auto it = data->find (key);

    if (it == data->end () || it->type () == nlohmann::detail::value_t::null)
        return T::fromScalar (defaultValue);

    return T::fromJSON (*it, project);
}

template const CUserSettingBoolean* Core::jsonFindUserConfig (
    const nlohmann::json::const_iterator& data, const CProject& project, const char* key,
    CUserSettingBoolean::data_type defaultValue);
template const CUserSettingVector3* Core::jsonFindUserConfig (
    const nlohmann::json::const_iterator& data, const CProject& project, const char* key,
    CUserSettingVector3::data_type defaultValue);
template const CUserSettingFloat* Core::jsonFindUserConfig (
    const nlohmann::json::const_iterator& data, const CProject& project, const char* key,
    CUserSettingFloat::data_type defaultValue);

template <typename T> const T* Core::jsonFindUserConfig (
    const nlohmann::json& data, const CProject& project, const char* key, typename T::data_type defaultValue
) {
    const auto it = data.find (key);

    if (it == data.end () || it->type () == nlohmann::detail::value_t::null)
        return T::fromScalar (defaultValue);

    return T::fromJSON (*it, project);
}

template const CUserSettingBoolean* Core::jsonFindUserConfig (
    const nlohmann::json& data, const CProject& project, const char* key, CUserSettingBoolean::data_type defaultValue);
template const CUserSettingVector3* Core::jsonFindUserConfig (
    const nlohmann::json& data, const CProject& project, const char* key, CUserSettingVector3::data_type defaultValue);
template const CUserSettingFloat* Core::jsonFindUserConfig (
    const nlohmann::json& data, const CProject& project, const char* key, CUserSettingFloat::data_type defaultValue);
