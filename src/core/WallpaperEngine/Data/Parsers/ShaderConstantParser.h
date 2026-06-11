#pragma once

#include "../JSON.h"
#include "../Model/Types.h"

namespace WallpaperEngine::Data::Parsers {
using JSON = WallpaperEngine::Data::JSON::JSON;
using namespace WallpaperEngine::Data::Model;

class ShaderConstantParser {
public:
    static ShaderConstantMap parse (const JSON& it, const Project& project);
};
} // namespace WallpaperEngine::Data::Parsers
