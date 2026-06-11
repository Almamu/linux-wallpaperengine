#pragma once

#include "../../../../common/WallpaperEngine/Logging/Log.h"
#include "../JSON.h"
#include "../Model/DynamicValue.h"
#include "../Model/Types.h"
#include "../Model/UserSetting.h"

namespace WallpaperEngine::Data::Parsers {
using JSON = WallpaperEngine::Data::JSON::JSON;
using namespace WallpaperEngine::Data::Model;

class UserSettingParser {
public:
    static UserSettingUniquePtr parse (const JSON& data, const Properties& properties, bool expectColor = false);
};
} // namespace WallpaperEngine::Data::Parsers
