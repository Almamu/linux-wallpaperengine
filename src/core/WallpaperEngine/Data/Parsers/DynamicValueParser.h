#pragma once

#include "../JSON.h"
#include "../Model/Types.h"

namespace WallpaperEngine::Data::Parsers {
using json = WallpaperEngine::Data::JSON::JSON;
using namespace WallpaperEngine::Data::Model;

class DynamicValueParser {
public:
    static Model::DynamicValueUniquePtr parse (const json& data, const Properties& properties, bool expectColor);
};
}