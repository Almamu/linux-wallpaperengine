#pragma once

#include <glm/fwd.hpp>
#include <string>

#include "WallpaperEngine/Data/Model/Types.h"
#include "WallpaperEngine/Data/Model/UserSetting.h"

namespace WallpaperEngine::Data::Builders {
using namespace WallpaperEngine::Data::Model;
class UserSettingBuilder {
public:
    template <typename T> static UserSettingUniquePtr fromValue (T defaultValue) {
	return fromDefault (defaultValue);
    }

private:
    // Match DynamicValue's value constructors so callers keep their existing conversions.
    static UserSettingUniquePtr fromDefault (const DynamicValue& value);
    static UserSettingUniquePtr fromDefault (const glm::vec4& value);
    static UserSettingUniquePtr fromDefault (const glm::vec3& value);
    static UserSettingUniquePtr fromDefault (const glm::vec2& value);
    static UserSettingUniquePtr fromDefault (float value);
    static UserSettingUniquePtr fromDefault (int value);
    static UserSettingUniquePtr fromDefault (bool value);
    static UserSettingUniquePtr fromDefault (const std::string& value);
    static UserSettingUniquePtr fromDefault (const Color& value);
};
} // namespace WallpaperEngine::Data::Builders
