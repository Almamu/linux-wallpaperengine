#include "ColorBuilder.h"
#include "VectorBuilder.h"

#include <algorithm>
#include <format>
#include <glm/vec3.hpp>

const WallpaperEngine::Data::Model::Color WallpaperEngine::Data::Builders::ColorBuilder::White = WallpaperEngine::Data::Model::Color (1.0f, 1.0f, 1.0f, 1.0f);
const WallpaperEngine::Data::Model::Color WallpaperEngine::Data::Builders::ColorBuilder::Black = WallpaperEngine::Data::Model::Color (0.0f, 0.0f, 0.0f, 1.0f);

WallpaperEngine::Data::Model::Color WallpaperEngine::Data::Builders::ColorBuilder::parse(const std::string& value, float alpha) {
    auto copy = value;

    // replace the actual separators with spaces to normalize them
    if (copy.find (',') != std::string::npos) {
	// replace comma separator with spaces so it's
	std::ranges::replace (copy, ',', ' ');
    }

    // hex colors should be converted to int colors
    if (copy.find ('#') == 0) {
	auto number = copy.substr (1);

        // expand short css notation into the right one
	// support for css notation
	if (number.size () == 3) {
	    number = std::format ("{}{}{}{}{}{}{:02x}", number.at(0), number.at(0), number.at(1), number.at(1), number.at(2), number.at(2), static_cast<int> (alpha * 255));
	} else if (number.size () == 4) {
	    number = std::format ("{}{}{}{}{}{}{}{}", number.at(0), number.at(0), number.at(1), number.at(1), number.at(2), number.at(2), number.at(3), number.at(3));
	} else if (number.size () != 6 && number.size () != 8) {
	    sLog.exception ("Invalid CSS color notation for ", value);
	}

        // parse hex color
	const auto color = std::stoi (number, nullptr, 16);

	return WallpaperEngine::Data::Model::Color (
	    (color >> 24 & 0xFF) / 255.0f,
	    (color >> 16 & 0xFF) / 255.0f,
	    (color >> 8 & 0xFF) / 255.0f,
	    (color & 0xFF) / 255.0f
	);
    }

    if (copy.find ('.') == std::string::npos) {
        glm::ivec4 final;

        try {
            final = VectorBuilder::parse<glm::ivec4> (copy);
        } catch (const std::exception&) {
            final = glm::ivec4(VectorBuilder::parse<glm::ivec3> (copy), alpha * 255);
        }

        return {
            final.r / 255.0f,
            final.g / 255.0f,
            final.b / 255.0f,
            final.a / 255.0f
        };
    }

    // this should be already a normalized color, so attempting vec4 and falling back to vec3 should be safe
    try {
        return WallpaperEngine::Data::Model::Color(VectorBuilder::parse<glm::vec4> (copy));
    } catch (const std::exception&) {
        return WallpaperEngine::Data::Model::Color(VectorBuilder::parse<glm::vec3> (copy), alpha);
    }
}