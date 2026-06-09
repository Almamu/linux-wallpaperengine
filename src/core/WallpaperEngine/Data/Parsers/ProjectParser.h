#pragma once

#include <nlohmann/json.hpp>

#include "../JSON.h"
#include "../Model/Project.h"
#include "WallpaperEngine/Assets/AssetLocator.h"

namespace WallpaperEngine::Data::Parsers {
using JSON = WallpaperEngine::Data::JSON::JSON;

using namespace WallpaperEngine::Assets;
using namespace WallpaperEngine::Data::Model;

/**
 * Parses a project file and returns an object representing it
 */
class ProjectParser {
public:
    static ProjectUniquePtr parse (const JSON& data, AssetLocatorUniquePtr container);

private:
    static Project::Type parseType (const std::string& type);
    static Properties parseProperties (const std::optional<JSON>& data);
};
} // namespace WallpaperEngine::Data::Parsers
