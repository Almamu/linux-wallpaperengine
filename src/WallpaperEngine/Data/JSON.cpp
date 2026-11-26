#include "JSON.h"

#include "WallpaperEngine/Data/Builders/ColorBuilder.h"
#include "WallpaperEngine/Data/Parsers/UserSettingParser.h"

#include "WallpaperEngine/Logging/Log.h"

#include <cctype>

using namespace WallpaperEngine::Data::JSON;
using namespace WallpaperEngine::Data::Model;
using namespace WallpaperEngine::Data::Parsers;

Color JsonExtensions::get () const { return ColorBuilder::parse (this->base ().get<std::string> ()); }

UserSettingUniquePtr JsonExtensions::color (const std::string& key, const Properties& properties, Color defaultValue) const {
    if (!this->optional (key).has_value ()) {
	return UserSettingBuilder::fromValue<Color> (defaultValue);
    }
    return this->color (key, properties);
}

namespace {
struct CompatibleJSON {
    std::string contents;
    bool changed { false };
    bool unterminatedBlockComment { false };
};

CompatibleJSON sanitizeCompatibleJSON (const std::string_view contents) {
    CompatibleJSON result { .contents = std::string (contents) };
    bool inString = false;
    bool escaped = false;

    // Strip comments first so the trailing-comma pass can treat comments before
    // a closing bracket exactly like whitespace. Replacing bytes with spaces
    // preserves line and column positions in any parse error.
    for (size_t index = 0; index < result.contents.size (); index++) {
	const char current = result.contents[index];

	if (inString) {
	    if (escaped) {
		escaped = false;
	    } else if (current == '\\') {
		escaped = true;
	    } else if (current == '"') {
		inString = false;
	    }
	    continue;
	}

	if (current == '"') {
	    inString = true;
	    continue;
	}

	if (current != '/' || index + 1 >= result.contents.size ()) {
	    continue;
	}

	const char next = result.contents[index + 1];
	if (next == '/') {
	    result.changed = true;
	    result.contents[index++] = ' ';
	    result.contents[index] = ' ';
	    while (++index < result.contents.size () && result.contents[index] != '\n'
		   && result.contents[index] != '\r') {
		result.contents[index] = ' ';
	    }
	} else if (next == '*') {
	    result.changed = true;
	    result.contents[index++] = ' ';
	    result.contents[index] = ' ';
	    bool terminated = false;
	    while (++index < result.contents.size ()) {
		if (result.contents[index] == '*' && index + 1 < result.contents.size ()
		    && result.contents[index + 1] == '/') {
		    result.contents[index++] = ' ';
		    result.contents[index] = ' ';
		    terminated = true;
		    break;
		}
		if (result.contents[index] != '\n' && result.contents[index] != '\r') {
		    result.contents[index] = ' ';
		}
	    }
	    if (!terminated) {
		result.unterminatedBlockComment = true;
		return result;
	    }
	}
    }

    inString = false;
    escaped = false;
    for (size_t index = 0; index < result.contents.size (); index++) {
	const char current = result.contents[index];
	if (inString) {
	    if (escaped) {
		escaped = false;
	    } else if (current == '\\') {
		escaped = true;
	    } else if (current == '"') {
		inString = false;
	    }
	    continue;
	}
	if (current == '"') {
	    inString = true;
	    continue;
	}
	if (current == ',') {
	    size_t next = index + 1;
	    while (next < result.contents.size ()
		   && std::isspace (static_cast<unsigned char> (result.contents[next]))) {
		next++;
	    }
	    if (next < result.contents.size () && (result.contents[next] == ']' || result.contents[next] == '}')) {
		result.changed = true;
		result.contents[index] = ' ';
	    }
	}
    }

    return result;
}
}

JSON WallpaperEngine::Data::JSON::parseCompatible (const std::string_view contents, const std::string_view source) {
    try {
	return JSON::parse (contents);
    } catch (const JSON::parse_error&) {
	auto compatible = sanitizeCompatibleJSON (contents);
	if (!compatible.changed || compatible.unterminatedBlockComment) {
	    throw;
	}

	auto result = JSON::parse (compatible.contents);
	sLog.out (
	    "Accepted Wallpaper Engine JSON comments/trailing commas", source.empty () ? "" : " in ",
	    source.empty () ? "" : std::string (source)
	);
	return result;
    }
}

UserSettingUniquePtr JsonExtensions::user (const std::string& key, const Properties& properties) const {
    const auto value = this->require (key, "User setting without default value must be present");

    return UserSettingParser::parse (value, properties);
}

UserSettingUniquePtr JsonExtensions::color (const std::string& key, const Properties& properties) const {
    const auto value = this->require (key, "User setting without default value must be present");

    return UserSettingParser::parse (value, properties, true);
}
