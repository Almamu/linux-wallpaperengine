#pragma once

#include "WallpaperEngine/Data/JSON.h"
#include "WallpaperEngine/Data/Model/Types.h"

namespace WallpaperEngine::Data::Parsers {
using JSON = WallpaperEngine::Data::JSON::JSON;
using namespace WallpaperEngine::Data::Model;

class WallpaperParser {
public:
    static WallpaperUniquePtr parse (const JSON& file, Project& project);

private:
    static SceneUniquePtr parseScene (const JSON& file, Project& project);
    static VideoUniquePtr parseVideo (const JSON& file, Project& project);
    static WebUniquePtr parseWeb (const JSON& file, Project& project);
    static ObjectList parseObjects (const JSON& objects, const Project& project);
};
} // namespace WallpaperEngine::Data::Parsers
