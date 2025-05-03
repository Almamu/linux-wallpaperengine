#pragma once

#include "WallpaperEngine/Data/JSON.h"
#include "WallpaperEngine/Data/Model/Types.h"
#include "WallpaperEngine/Data/Model/UserSetting.h"
#include "WallpaperEngine/Logging/CLog.h"

namespace WallpaperEngine::Data::Parsers {
using json = WallpaperEngine::Data::JSON::JSON;
using namespace WallpaperEngine::Data::Model;

class UserSettingParser {
  public:
    static UserSettingSharedPtr parse (const json& data, const Properties& properties);
};
} // namespace WallpaperEngine::Data::Parsers
