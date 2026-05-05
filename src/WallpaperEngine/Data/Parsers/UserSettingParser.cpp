#include "UserSettingParser.h"

#include "WallpaperEngine/Data/Model/Property.h"
#include "WallpaperEngine/Data/Model/ScriptedDynamicValue.h"
#include "WallpaperEngine/Data/Model/UserSetting.h"
#include "WallpaperEngine/Logging/Log.h"

using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Builders;

UserSettingUniquePtr UserSettingParser::parse (const json& data, const Properties& properties) {
    auto value = std::make_unique<DynamicValue> ();
    PropertySharedPtr property;
    std::optional<ConditionInfo> condition;
    auto valueIt = data;
    std::string content = data.dump ();

    std::optional<std::string> scriptSource;
    std::optional<json> scriptPropsJson;

    if (data.is_object ()) {
	const auto user = data.optional ("user");
	const auto script = data.optional ("script");
	valueIt = data.require ("value", "User setting must have a value");

	if (script.has_value () && !script->is_null ()) {
	    scriptSource = script->get<std::string> ();
	    scriptPropsJson = data.optional ("scriptproperties");
	}

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

    // actual value parsing
    if (valueIt.is_string ()) {
	std::string str = valueIt;

	// TODO: VALIDATE THIS IS RIGHT?
	if (int size = VectorBuilder::preparseSize (str); size == 2) {
	    value->update (static_cast<glm::vec2> (valueIt));
	} else if (size == 3) {
	    value->update (static_cast<glm::vec3> (valueIt));
	} else if (size == 4) {
	    value->update (static_cast<glm::vec4> (valueIt));
	} else {
	    // preparseSize returned 0: no spaces found — try parsing as a scalar float,
	    // fall back to a plain string for non-numeric values (e.g. "bottom", "center").
	    std::size_t parsed = 0;
	    try {
		float f = std::stof (str, &parsed);
		if (parsed == str.size ()) {
		    value->update (f);
		} else {
		    value->update (str);
		}
	    } catch (const std::exception&) {
		value->update (str);
	    }
	}
    } else if (valueIt.is_number_integer ()) {
	value->update (valueIt.get<int> ());
    } else if (valueIt.is_number_float ()) {
	value->update (valueIt.get<float> ());
    } else if (valueIt.is_boolean ()) {
	value->update (valueIt.get<bool> ());
    } else if (valueIt.is_null ()) {
	// null value with no connection to property
	value->update ();
    }

    // If the setting has a script, wrap the base value in a ScriptedDynamicValue
    if (scriptSource.has_value ()) {
	std::map<std::string, DynamicValueUniquePtr> scriptProps;

	if (scriptPropsJson.has_value () && scriptPropsJson->is_object ()) {
	    for (const auto& [key, propData] : scriptPropsJson->items ()) {
		auto propSetting = UserSettingParser::parse (propData, properties);
		scriptProps[key] = std::move (propSetting->value);
	    }
	}

	value = std::make_unique<ScriptedDynamicValue> (
	    std::move (scriptSource.value ()),
	    std::move (scriptProps),
	    std::move (*value)
	);
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
