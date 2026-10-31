#include "ColorBuilder.h"
#include "VectorBuilder.h"

#include <format>
#include <glm/vec3.hpp>

const WallpaperEngine::Data::Model::Color WallpaperEngine::Data::Builders::ColorBuilder::White
    = WallpaperEngine::Data::Model::Color (1.0f, 1.0f, 1.0f, 1.0f);
const WallpaperEngine::Data::Model::Color WallpaperEngine::Data::Builders::ColorBuilder::Black
    = WallpaperEngine::Data::Model::Color (0.0f, 0.0f, 0.0f, 1.0f);

WallpaperEngine::Data::Model::Color
WallpaperEngine::Data::Builders::ColorBuilder::parse (const std::string& value, float alpha) {
    // hex colors should be converted to int colors
    if (value.find ('#') == 0) {
	auto number = value.substr (1);

	// expand short css notation into the right one
	// support for css notation
	if (number.size () == 3) {
	    number = std::format (
		"{}{}{}{}{}{}{:02x}", number.at (0), number.at (0), number.at (1), number.at (1), number.at (2),
		number.at (2), static_cast<int> (alpha * 255)
	    );
	} else if (number.size () == 4) {
	    number = std::format (
		"{}{}{}{}{}{}{}{}", number.at (0), number.at (0), number.at (1), number.at (1), number.at (2),
		number.at (2), number.at (3), number.at (3)
	    );
	} else if (number.size () != 6 && number.size () != 8) {
	    sLog.exception ("Invalid CSS color notation for ", value);
	}

	// parse hex color
	const auto color = std::stoi (number, nullptr, 16);

	return WallpaperEngine::Data::Model::Color (
	    (color >> 24 & 0xFF) / 255.0f, (color >> 16 & 0xFF) / 255.0f, (color >> 8 & 0xFF) / 255.0f,
	    (color & 0xFF) / 255.0f
	);
    }

    int vectorSize = VectorBuilder::preparseSize (value);

    if (vectorSize != 3 && vectorSize != 4) {
	throw std::invalid_argument ("Invalid color value");
    }

    if (value.find ('.') == std::string::npos) {
	const auto final = vectorSize == 3 ? glm::ivec4 (VectorBuilder::parse<glm::ivec3> (value), alpha * 255)
					   : VectorBuilder::parse<glm::ivec4> (value);

	return { final.r / 255.0f, final.g / 255.0f, final.b / 255.0f, final.a / 255.0f };
    }

    return Model::Color (
	vectorSize == 3 ? glm::vec4 (VectorBuilder::parse<glm::vec3> (value), alpha)
			: VectorBuilder::parse<glm::vec4> (value)
    );
}