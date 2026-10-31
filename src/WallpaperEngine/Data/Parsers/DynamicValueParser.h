#pragma once

#include "WallpaperEngine/Data/JSON.h"
#include "WallpaperEngine/Data/Model/Types.h"

namespace WallpaperEngine::Data::Parsers {
using json = WallpaperEngine::Data::JSON::JSON;
using namespace WallpaperEngine::Data::Model;

/**
 * What a setting's string value should be read as.
 * Any guesses from the contents ("1 2" becomes a vec2), Text always keeps the string.
 */
enum class ValueKind {
    Any,
    Color,
    Text,
};

class DynamicValueParser {
public:
    static Model::DynamicValueUniquePtr parse (const json& data, const Properties& properties, ValueKind kind);
};
}