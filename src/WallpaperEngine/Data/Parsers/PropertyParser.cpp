#include "PropertyParser.h"
#include "../Model/Property.h"

using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Model;

PropertySharedPtr PropertyParser::parse (const JSON& it, const std::string& name) {
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
        return parseCombo (it, name);
    }
    if (type == "text") {
        return parseText (it, name);
    }
    if (type == "scenetexture") {
        return parseSceneTexture (it, name);
    }
    if (type == "file") {
        return parseFile (it, name);
    }
    if (type == "textinput") {
        return parseTextInput (it, name);
    }

    if (type.has_value () && type != "group") {
        // show the error and ignore this property
        sLog.error ("Unexpected type for property: ", type);
        sLog.error (it.dump ());
    }

    return nullptr;
}


PropertySharedPtr PropertyParser::parseCombo (const JSON& it, const std::string& name) {
    std::map <std::string, std::string> optionsMap = {};

    const auto options = it.require ("options", "Combo property must have options");

    if (!options.is_array ()) {
        sLog.exception ("Property combo options should be an array");
    }

    for (auto& cur : options) {
        if (!cur.is_object ()) {
            continue;
        }

        const auto value = cur.require ("value", "Combo option must have a value");

        optionsMap.emplace (
            value.is_number () ? std::to_string (value.get <int> ()) : value.get <std::string> (),
            cur.require ("label", "Combo option must have a label")
        );
    }

    const auto value = it.require ("value", "Combo property must have a value");

    return std::make_shared <PropertyCombo> (PropertyData {
        .name = name,
        .text = it.optional <std::string> ("text", ""),
    }, ComboData {
        .values = optionsMap
    }, value.is_number () ? std::to_string (value.get <int> ()) : value.get <std::string> ());
}

PropertySharedPtr PropertyParser::parseColor (const JSON& it, const std::string& name) {
    return std::make_shared <PropertyColor> (PropertyData {
        .name = name,
        .text = it.optional <std::string> ("text", ""),
    }, it.require ("value", "Property must have a value"));
}

PropertySharedPtr PropertyParser::parseBoolean (const JSON& it, const std::string& name) {
    return std::make_shared <PropertyBoolean> (PropertyData {
        .name = name,
        .text = it.optional <std::string> ("text", ""),
    }, it.require ("value", "Property must have a value"));
}

PropertySharedPtr PropertyParser::parseSlider (const JSON& it, const std::string& name) {
    return std::make_shared <PropertySlider> (PropertyData {
        .name = name,
        .text = it.optional <std::string> ("text", ""),
    }, SliderData {
        .min = it.optional ("min", 0.0f),
        .max = it.optional ("max", 0.0f),
        .step = it.optional ("step", 0.0f),
    }, it.require ("value", "Property must have a value"));
}

PropertySharedPtr PropertyParser::parseText (const JSON& it, const std::string& name) {
    return std::make_shared <PropertyText> (PropertyData {
        .name = name,
        .text = it.optional <std::string> ("text", ""),
    });
}

PropertySharedPtr PropertyParser::parseSceneTexture (const JSON& it, const std::string& name) {
    return std::make_shared <PropertySceneTexture> (PropertyData {
        .name = name,
        .text = it.optional <std::string> ("text", ""),
    }, it.require ("value", "Property must have a value"));
}

PropertySharedPtr PropertyParser::parseFile (const JSON& it, const std::string& name) {
    return std::make_shared <PropertyFile> (PropertyData {
        .name = name,
        .text = it.optional <std::string> ("text", ""),
    }, it.optional <std::string> ("value", ""));
}

PropertySharedPtr PropertyParser::parseTextInput (const JSON& it, const std::string& name) {
    return std::make_shared <PropertyTextInput> (PropertyData {
        .name = name,
        .text = it.optional <std::string> ("text", ""),
    }, it.require ("value", "Property must have a value").dump ());
}