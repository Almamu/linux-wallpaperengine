#pragma once

#include <nlohmann/json.hpp>

#include "WallpaperEngine/Data/JSON.h"
#include "WallpaperEngine/Data/Model/Project.h"

namespace WallpaperEngine::Data::Parsers {
using JSON = WallpaperEngine::Data::JSON::JSON;

using namespace WallpaperEngine::Data::Model;

/**
 * Parses a project file and returns an object representing it
 */
class ProjectParser {
  public:
    static ProjectSharedPtr parse (const JSON& data, const ContainerWeakPtr& container);

  private:
    static Project::Type parseType (const std::string& type);
    static Properties parseProperties (const std::optional <JSON>& data);
};
} // namespace WallpaperEngine::Data::Parsers
