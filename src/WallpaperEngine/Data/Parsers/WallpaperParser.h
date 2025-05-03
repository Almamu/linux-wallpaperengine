#pragma once

#include "WallpaperEngine/Data/JSON.h"
#include "WallpaperEngine/Data/Model/Types.h"

namespace WallpaperEngine::Data::Parsers {
using JSON = WallpaperEngine::Data::JSON::JSON;
using namespace WallpaperEngine::Data::Model;

class WallpaperParser {
  public:
    static WallpaperSharedPtr parse (const JSON& file, const ProjectWeakPtr& project);

  private:
    static SceneSharedPtr parseScene (const JSON& file, const ProjectWeakPtr& project);
    static VideoSharedPtr parseVideo (const JSON& file, const ProjectWeakPtr& project);
    static WebSharedPtr parseWeb (const JSON& file, const ProjectWeakPtr& project);
    static ObjectMap parseObjects (const JSON& objects, const ProjectWeakPtr& project);
};
} // namespace WallpaperEngine::Data::Parsers
