#pragma once

#include "WallpaperEngine/Data/JSON.h"
#include "WallpaperEngine/Data/Model/DynamicValue.h"
#include "WallpaperEngine/Data/Model/Types.h"
#include "WallpaperEngine/Data/Model/UserSetting.h"
#include "WallpaperEngine/Logging/Log.h"

namespace WallpaperEngine::Data::Parsers {
using json = WallpaperEngine::Data::JSON::JSON;
using namespace WallpaperEngine::Data::Model;

class UserSettingParser {
public:
    static UserSettingUniquePtr
    parse (const json& data, const Properties& properties, int objectId = -1, const std::string& objectName = "");
};
} // namespace WallpaperEngine::Data::Parsers
