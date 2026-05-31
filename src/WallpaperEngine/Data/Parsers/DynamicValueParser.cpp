#include "DynamicValueParser.h"
#include "WallpaperEngine/Data/Model/DynamicValue.h"

using namespace WallpaperEngine::Data::Parsers;

DynamicValueUniquePtr
DynamicValueParser::parse (const json& data, const Properties properties, int objectId, const std::string& objectName) {
    auto value = std::make_unique<DynamicValue> ();
    auto valueIt = data;
    std::optional<std::string> scriptSource = std::nullopt;
    std::optional<json> scriptPropsJson = std::nullopt;

    if (data.is_object ()) {
	const auto user = data.optional ("user");
	const auto script = data.optional ("script");
	valueIt = data.require ("value", "User setting must have a value");

	if (script.has_value () && !script->is_null ()) {
	    scriptSource = script->get<std::string> ();
	    scriptPropsJson = data.optional ("scriptproperties");
	}
    }

    // actual value parsing
    if (valueIt.is_string ()) {
	// TODO: THINK ABOUT HOW TO APPROACH COLOR PARSING OR ENFORCING IT AFTER
	std::string str = valueIt;
	int size = Builders::VectorBuilder::preparseSize (str);

	if (size == 1) {
	    // scalar? text value?
	    std::size_t parsed = 0;
	    try {
		float f = std::stof (str, &parsed);

		if (parsed == str.size ()) {
		    value->update (f, DynamicValue::UpdateSource::Initialization);
		} else {
		    value->update (str, DynamicValue::UpdateSource::Initialization);
		}
	    } catch (const std::exception&) {
		value->update (str, DynamicValue::UpdateSource::Initialization);
	    }
	} else if (size == 2) {
	    value->update (static_cast<glm::vec2> (valueIt), DynamicValue::UpdateSource::Initialization);
	} else if (size == 3) {
	    value->update (static_cast<glm::vec3> (valueIt), DynamicValue::UpdateSource::Initialization);
	} else {
	    value->update (static_cast<glm::vec4> (valueIt), DynamicValue::UpdateSource::Initialization);
	}
    } else if (valueIt.is_number_integer ()) {
	value->update (valueIt.get<int> (), DynamicValue::UpdateSource::Initialization);
    } else if (valueIt.is_number_float ()) {
	value->update (valueIt.get<float> (), DynamicValue::UpdateSource::Initialization);
    } else if (valueIt.is_boolean ()) {
	value->update (valueIt.get<bool> (), DynamicValue::UpdateSource::Initialization);
    } else if (valueIt.is_null ()) {
	// null value with no connection to property
	value->update (DynamicValue::UpdateSource::Initialization);
    }

    if (scriptSource.has_value ()) {
	std::map<std::string, DynamicValue> scriptProps;

	if (scriptPropsJson.has_value () && scriptPropsJson->is_object ()) {
	    for (const auto& [key, propData] : scriptPropsJson->items ()) {
		scriptProps[key] = *parse (propData, properties, objectId, objectName);
	    }
	}

	value->setProperties (scriptProps);
	value->setScriptSource (scriptSource.value ());
    }

    return value;
}