#include "UserSettingParser.h"
#include "DynamicValueParser.h"

#include "WallpaperEngine/Data/Model/Property.h"
#include "WallpaperEngine/Data/Model/UserSetting.h"

using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Builders;

UserSettingUniquePtr UserSettingParser::parse (const json& data, const Properties& properties, bool expectColor) {
    auto value = DynamicValueParser::parse (data, properties, expectColor);
    PropertySharedPtr property;
    std::optional<ConditionInfo> condition = std::nullopt;

    if (data.is_object ()) {
	const auto user = data.optional ("user");

	if (user.has_value () && !user->is_null ()) {
	    std::string source;

	    if (const auto& it = *user; it.is_string ()) {
		source = it;
	    } else {
		condition = ConditionInfo {
		    .name = it.require<std::string> ("name", "Name for conditional setting must be present"),
		    .condition
		    = it.require<std::string> ("condition", "Condition for conditional setting must be present"),
		};

		source = condition.value ().name;
	    }

	    if (const auto propertyIt = properties.find (source); propertyIt != properties.end ()) {
		property = propertyIt->second;
	    }
	}
    }

    // TODO: This might need to be removed if it causes issues with default values
    // Connect to property if one is specified (this allows property overrides to propagate)
    if (property != nullptr) {
	if (condition.has_value ()) {
	    value->attachCondition (condition.value ());
	}

	value->connect (property.get ());
    }

    return std::make_unique<UserSetting> (UserSetting {
	.value = std::move (value),
	.property = property,
	.condition = condition,
    });
}
