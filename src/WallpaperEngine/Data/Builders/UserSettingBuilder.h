#pragma once

#include "WallpaperEngine/Data/Model/Types.h"
#include "WallpaperEngine/Data/Model/UserSetting.h"

namespace WallpaperEngine::Data::Builders {
using namespace WallpaperEngine::Data::Model;
class UserSettingBuilder {
  public:
    template <typename T>
    static UserSettingSharedPtr fromValue (T defaultValue) {
        DynamicValueUniquePtr value = std::make_unique <DynamicValue> ();

        value->update (defaultValue);

        return std::make_shared <UserSetting> (UserSetting {
            .value = std::move (value),
            .property = PropertyWeakPtr (),
            .condition = std::nullopt,
        });
    }
};
} // namespace WallpaperEngine::Data::Builders