#include "PropertyParser.h"
#include "../Model/Property.h"

#include <algorithm>
#include <optional>

using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Model;

namespace {
std::string comboValueKey (const JSON& value, bool preserveNumber) {
    if (value.is_number ()) {
	return preserveNumber ? value.dump () : std::to_string (value.get<int> ());
    }
    if (value.is_boolean ()) {
	return value.get<bool> () ? "true" : "false";
    }
    return value.get<std::string> ();
}
} // namespace

PropertySharedPtr PropertyParser::parse (const JSON& it, const std::string& name, bool web) {
    // type might not be included, in which case means the same as a group
    const auto type = it.optional ("type");

    if (type == "color") {
	return parseColor (it, name);
    }
    if (type == "bool") {
	return parseBoolean (it, name);
    }
    if (type == "slider") {
	return parseSlider (it, name);
    }
    if (type == "combo") {
	return parseCombo (it, name, web);
    }
    if (type == "text") {
	return parseText (it, name);
    }
    if (type == "scenetexture") {
	return parseSceneTexture (it, name);
    }
    if (type == "file" || type == "directory") {
	return parseFile (it, name);
    }
    if (type == "textinput") {
	return parseTextInput (it, name);
    }
    if (type == "usershortcut") {
	return parseTextInput (it, name);
    }

    if (type.has_value () && type != "group") {
	// show the error and ignore this property
	sLog.error ("Unexpected type for property: ", type);
	sLog.error (it.dump ());
    }

    return nullptr;
}

PropertySharedPtr PropertyParser::parseCombo (const JSON& it, const std::string& name, bool web) {
    const auto options = it.require ("options", "Combo property must have options");

    if (!options.is_array ()) {
	sLog.exception ("Property combo options should be an array");
    }

    // Web scripts receive typed option values. Keep the previous normalizing
    // behavior for ordinary scene combo bindings.
    const bool preserveTypes = web || std::ranges::any_of (options, [] (const JSON& option) {
	const auto value = option.find ("value");
	return value != option.end () && value->is_boolean ();
    });
    const auto defaultValue = it.require ("value", "Combo property must have a value");
    auto defaultKey = comboValueKey (defaultValue, preserveTypes);
    std::optional<std::string> authoredOptionKey;
    ComboData combo;
    for (auto& cur : options) {
	if (!cur.is_object ()) {
	    continue;
	}

	const auto value = cur.require ("value", "Combo option must have a value");

	const auto [option, inserted] = combo.values.emplace (
	    comboValueKey (value, preserveTypes), cur.require ("label", "Combo option must have a label")
	);
	if (inserted && preserveTypes && (value.is_boolean () || value.is_number ())) {
	    combo.webLiteralValues.insert (option->first);
	}
	// JSON equality preserves boolean and string identity while matching 1 to 1.0.
	if (preserveTypes && value == defaultValue && (!authoredOptionKey || option->first == defaultKey)) {
	    authoredOptionKey = option->first;
	}
    }

    if (authoredOptionKey) {
	defaultKey = std::move (*authoredOptionKey);
	const bool literal = defaultValue.is_boolean () || defaultValue.is_number ();
	if (literal != combo.webLiteralValues.contains (defaultKey)) {
	    combo.authoredDefaultIsLiteral = literal;
	}
    }

    return std::make_shared<PropertyCombo> (
	PropertyData {
	    .name = name,
	    .text = it.optional<std::string> ("text", ""),
	},
	std::move (combo), defaultKey
    );
}

PropertySharedPtr PropertyParser::parseColor (const JSON& it, const std::string& name) {
    return std::make_shared<PropertyColor> (
	PropertyData {
	    .name = name,
	    .text = it.optional<std::string> ("text", ""),
	},
	it.require ("value", "Property must have a value")
    );
}

PropertySharedPtr PropertyParser::parseBoolean (const JSON& it, const std::string& name) {
    return std::make_shared<PropertyBoolean> (
	PropertyData {
	    .name = name,
	    .text = it.optional<std::string> ("text", ""),
	},
	it.optional ("value", false)
    );
}

PropertySharedPtr PropertyParser::parseSlider (const JSON& it, const std::string& name) {
    return std::make_shared<PropertySlider> (
	PropertyData {
	    .name = name,
	    .text = it.optional<std::string> ("text", ""),
	},
	SliderData {
	    .min = it.optional ("min", 0.0f),
	    .max = it.optional ("max", 0.0f),
	    .step = it.optional ("step", 0.0f),
	},
	it.require ("value", "Property must have a value")
    );
}

PropertySharedPtr PropertyParser::parseText (const JSON& it, const std::string& name) {
    return std::make_shared<PropertyText> (PropertyData {
	.name = name,
	.text = it.optional<std::string> ("text", ""),
    });
}

PropertySharedPtr PropertyParser::parseSceneTexture (const JSON& it, const std::string& name) {
    return std::make_shared<PropertySceneTexture> (
	PropertyData {
	    .name = name,
	    .text = it.optional<std::string> ("text", ""),
	},
	it.require ("value", "Property must have a value")
    );
}

PropertySharedPtr PropertyParser::parseFile (const JSON& it, const std::string& name) {
    return std::make_shared<PropertyFile> (
	PropertyData {
	    .name = name,
	    .text = it.optional<std::string> ("text", ""),
	},
	it.optional<std::string> ("value", "")
    );
}

PropertySharedPtr PropertyParser::parseTextInput (const JSON& it, const std::string& name) {
    return std::make_shared<PropertyTextInput> (
	PropertyData {
	    .name = name,
	    .text = it.optional<std::string> ("text", ""),
	},
	// value is optional and read as a raw string: require+.dump() crashed on absent
	// values and wrapped present ones in literal JSON quotes (matches parseFile above,
	// and upstream Almamu PR #625)
	it.optional<std::string> ("value", "")
    );
}
