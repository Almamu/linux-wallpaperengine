#include "UserSettingBuilder.h"

#include "WallpaperEngine/Data/Model/Color.h"
#include "WallpaperEngine/Data/Model/DynamicValue.h"

using namespace WallpaperEngine::Data::Builders;

namespace {
template <typename T> UserSettingUniquePtr buildSetting (const T& value) {
    return std::make_unique<UserSetting> (UserSetting {
	.value = std::make_unique<DynamicValue> (value),
	.property = PropertySharedPtr (),
	.condition = std::nullopt,
    });
}
} // namespace

UserSettingUniquePtr UserSettingBuilder::fromDefault (const DynamicValue& value) { return buildSetting (value); }
UserSettingUniquePtr UserSettingBuilder::fromDefault (const glm::vec4& value) { return buildSetting (value); }
UserSettingUniquePtr UserSettingBuilder::fromDefault (const glm::vec3& value) { return buildSetting (value); }
UserSettingUniquePtr UserSettingBuilder::fromDefault (const glm::vec2& value) { return buildSetting (value); }
UserSettingUniquePtr UserSettingBuilder::fromDefault (float value) { return buildSetting (value); }
UserSettingUniquePtr UserSettingBuilder::fromDefault (int value) { return buildSetting (value); }
UserSettingUniquePtr UserSettingBuilder::fromDefault (bool value) { return buildSetting (value); }
UserSettingUniquePtr UserSettingBuilder::fromDefault (const std::string& value) { return buildSetting (value); }
UserSettingUniquePtr UserSettingBuilder::fromDefault (const Color& value) { return buildSetting (value); }
