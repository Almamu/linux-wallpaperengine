#pragma once

#include "WallpaperEngine/Data/JSON.h"
#include "WallpaperEngine/Data/Model/Types.h"

namespace WallpaperEngine::Data::Parsers {
using JSON = WallpaperEngine::Data::JSON::JSON;
using namespace WallpaperEngine::Data::Model;

class ModelParser {
  public:
    static ModelUniquePtr load (Project& project, const std::string& filename);
    static ModelUniquePtr parse (const JSON& file, Project& project, const std::string& filename);
};
} // namespace WallpaperEngine::Data::Parsers
