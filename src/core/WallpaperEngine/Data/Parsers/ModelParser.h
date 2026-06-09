#pragma once

#include "../JSON.h"
#include "../Model/Types.h"

namespace WallpaperEngine::Data::Parsers {
using JSON = WallpaperEngine::Data::JSON::JSON;
using namespace WallpaperEngine::Data::Model;

class ModelParser {
public:
    static ModelUniquePtr load (const Project& project, const std::string& filename);
    static ModelUniquePtr parse (const JSON& file, const Project& project, const std::string& filename);
};
} // namespace WallpaperEngine::Data::Parsers
