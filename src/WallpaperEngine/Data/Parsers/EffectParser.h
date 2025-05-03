#pragma once

#include "WallpaperEngine/Data/JSON.h"
#include "WallpaperEngine/Data/Model/Types.h"

namespace WallpaperEngine::Data::Parsers {
using JSON = WallpaperEngine::Data::JSON::JSON;
using namespace WallpaperEngine::Data::Model;

class EffectParser {
  public:
    static EffectUniquePtr load (const ProjectWeakPtr& project, const std::string& filename);

  private:
    static EffectUniquePtr parse (const JSON& it, const ProjectWeakPtr& project);
    static std::vector <std::string> parseDependencies (const JSON& it);
    static std::vector <EffectPassUniquePtr> parseEffectPasses (const JSON& it, const ProjectWeakPtr& project);
    static std::map <int, std::string> parseBinds (const JSON& it);
};
} // namespace WallpaperEngine::Data::Parsers
