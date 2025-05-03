#include "JSON.h"

#include "WallpaperEngine/Data/Parsers/UserSettingParser.h"

using namespace WallpaperEngine::Data::JSON;
using namespace WallpaperEngine::Data::Model;

UserSettingSharedPtr JsonExtensions::user (const std::string& key, const Properties& properties) const {
    const auto value = this->require (key, "User setting without default value must be present");

    return UserSettingParser::parse (value, properties);
}