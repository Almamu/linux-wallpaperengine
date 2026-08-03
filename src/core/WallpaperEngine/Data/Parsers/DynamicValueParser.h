#pragma once

#include "../JSON.h"
#include "../Model/Types.h"

namespace WallpaperEngine::Data::Parsers {
using JSON = WallpaperEngine::Data::JSON::JSON;
using namespace WallpaperEngine::Data::Model;

class DynamicValueParser {
public:
    static Model::DynamicValueUniquePtr parse (const JSON& data, const Properties& properties, bool expectColor);
};
}