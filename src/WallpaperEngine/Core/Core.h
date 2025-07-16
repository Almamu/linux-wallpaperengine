#pragma once

#include "CProject.h"
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <nlohmann/json.hpp>
#include <string>

namespace WallpaperEngine::Core {
class CProject;

glm::vec4 aToVector4 (const char* str);
glm::vec3 aToVector3 (const char* str);
glm::vec2 aToVector2 (const char* str);

glm::vec4 aToVector4 (const std::string& str);
glm::vec3 aToVector3 (const std::string& str);
glm::vec2 aToVector2 (const std::string& str);

glm::ivec4 aToVector4i (const char* str);
glm::ivec3 aToVector3i (const char* str);
glm::ivec2 aToVector2i (const char* str);

glm::ivec4 aToVector4i (const std::string& str);
glm::ivec3 aToVector3i (const std::string& str);
glm::ivec2 aToVector2i (const std::string& str);

glm::vec3 aToColorf (const char* str);
glm::vec3 aToColorf (const std::string& str);

glm::ivec3 aToColori (const char* str);
glm::ivec3 aToColori (const std::string& str);

template <typename T> const T jsonFindRequired (
    const nlohmann::json::const_iterator& data, const char* key, const char* notFoundMsg);
template <typename T> const T jsonFindRequired (
    const nlohmann::json& data, const char* key, const char* notFoundMsg);
nlohmann::json::const_iterator jsonFindRequired (
    const nlohmann::json::const_iterator& data, const char* key, const char* notFoundMsg);
nlohmann::json::const_iterator jsonFindRequired (
    const nlohmann::json& data, const char* key, const char* notFoundMsg);
template <typename T> const T jsonFindDefault (
    const nlohmann::json::const_iterator& data, const char* key, const T defaultValue);
template <typename T> const T jsonFindDefault (
    const nlohmann::json& data, const char* key, const T defaultValue);
template <typename T> const T* jsonFindUserConfig (
    const nlohmann::json::const_iterator& data, const CProject& project, const char* key, typename T::data_type defaultValue);
template <typename T> const T* jsonFindUserConfig (
    const nlohmann::json& data, const CProject& project, const char* key, typename T::data_type defaultValue);
} // namespace WallpaperEngine::Core
