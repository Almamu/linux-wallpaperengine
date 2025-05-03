#include "UserSettingParser.h"

#include "WallpaperEngine/Data/Model/UserSetting.h"

using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Builders;

UserSettingSharedPtr UserSettingParser::parse (const json& data, const Properties& properties) {
    DynamicValueUniquePtr value = std::make_unique <DynamicValue> ();
    PropertyWeakPtr property;
    std::optional<ConditionInfo> condition;
    auto valueIt = data;

    if (data.is_object ()) {
        const auto user = data.optional ("user");
        valueIt = data.require ("value", "User setting must have a value");

        if (user.has_value ()) {
            std::string source;
            const auto& it = user.value ();

            if (it.is_string ()) {
                source = it;
            } else {
                condition = ConditionInfo {
                    .name = it.require <std::string> ("name", "Name for conditional setting must be present"),
                    .condition = it.require <std::string> ("condition", "Condition for conditional setting must be present"),
                };

                source = condition.value ().name;
            }

            const auto propertyIt = properties.find (source);

            if (propertyIt != properties.end ()) {
                property = propertyIt->second;
            }
        }
    }

    // actual value parsing
    if (valueIt.is_string ()) {
        std::string str = valueIt;
        int size = VectorBuilder::preparseSize (str);

        //TODO: VALIDATE THIS IS RIGHT?
        if (size == 2) {
            value->update ((glm::vec2) valueIt);
        } else if (size == 3) {
            value->update ((glm::vec3) valueIt);
        } else {
            value->update ((glm::vec4) valueIt);
        }
    } else if (valueIt.is_number_integer ()) {
        value->update (valueIt.get <int> ());
    } else if (valueIt.is_number_float ()) {
        value->update (valueIt.get <float> ());
    } else if (valueIt.is_boolean ()) {
        value->update (valueIt.get <bool> ());
    } else {
        sLog.exception ("Unsupported user setting type ", valueIt.type_name ());
    }

    return std::make_shared <UserSetting> (UserSetting {
        .value = std::move (value),
        .property = property,
        .condition = condition,
    });
}