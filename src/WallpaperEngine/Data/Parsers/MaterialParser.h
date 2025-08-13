#pragma once

#include "WallpaperEngine/Data/JSON.h"
#include "WallpaperEngine/Data/Model/Types.h"

namespace WallpaperEngine::Data::Parsers {
using JSON = WallpaperEngine::Data::JSON::JSON;
using namespace WallpaperEngine::Data::Model;

class MaterialParser {
  public:
    static MaterialUniquePtr load (Project& project, const std::string& filename);
    static MaterialUniquePtr parse (const JSON& it, Project& project, const std::string& filename);
    static std::vector <MaterialPassUniquePtr> parsePasses (const JSON& it, Project& project);
    static MaterialPassUniquePtr parsePass (const JSON& it, Project& project);
    static std::map <int, std::string> parseTextures (const JSON& it);
    static std::map <std::string, int> parseCombos (const JSON& it);
};
} // namespace WallpaperEngine::Data::Parsers
