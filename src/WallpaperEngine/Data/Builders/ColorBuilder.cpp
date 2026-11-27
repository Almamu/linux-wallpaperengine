#include "ColorBuilder.h"
#include "VectorBuilder.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <glm/vec3.hpp>
#include <stdexcept>
#include <string_view>

const WallpaperEngine::Data::Model::Color WallpaperEngine::Data::Builders::ColorBuilder::White
    = WallpaperEngine::Data::Model::Color (1.0f, 1.0f, 1.0f, 1.0f);
const WallpaperEngine::Data::Model::Color WallpaperEngine::Data::Builders::ColorBuilder::Black
    = WallpaperEngine::Data::Model::Color (0.0f, 0.0f, 0.0f, 1.0f);

WallpaperEngine::Data::Model::Color
WallpaperEngine::Data::Builders::ColorBuilder::parse (const std::string& value, float alpha) {
    // Parse RGB, RGBA, RRGGBB or RRGGBBAA without signed integer overflow.
    if (value.starts_with ('#')) {
	const std::string_view digits (value.data () + 1, value.size () - 1);
	const bool shortForm = digits.size () == 3 || digits.size () == 4;
	const bool hasAlpha = digits.size () == 4 || digits.size () == 8;
	if (!shortForm && digits.size () != 6 && digits.size () != 8) {
	    throw std::invalid_argument ("Invalid CSS color notation: " + value);
	}

	std::uint32_t color = 0;
	const auto end = digits.data () + digits.size ();
	const auto result = std::from_chars (digits.data (), end, color, 16);
	if (result.ec != std::errc {} || result.ptr != end) {
	    throw std::invalid_argument ("Invalid CSS color notation: " + value);
	}

	const unsigned int bits = shortForm ? 4 : 8;
	const auto mask = (1u << bits) - 1u;
	if (!hasAlpha) {
	    color <<= bits;
	}
	const auto channel = [color, bits, mask] (unsigned int index) {
	    return static_cast<float> ((color >> (index * bits)) & mask) / mask;
	};
	return { channel (3), channel (2), channel (1), hasAlpha ? channel (0) : std::trunc (alpha * 255.0f) / 255.0f };
    }

    const int vectorSize = VectorBuilder::componentCount (value);

    if (vectorSize != 3 && vectorSize != 4) {
	throw std::invalid_argument ("Invalid color value");
    }

    auto parsedColor = vectorSize == 3 ? glm::vec4 (VectorBuilder::parse<glm::vec3> (value), alpha)
				       : VectorBuilder::parse<glm::vec4> (value);
    if (!std::isfinite (parsedColor.r) || !std::isfinite (parsedColor.g) || !std::isfinite (parsedColor.b)
	|| !std::isfinite (parsedColor.a)) {
	throw std::invalid_argument ("Invalid color value: " + value);
    }

    // Wallpaper Engine writes normalized values such as "1 1 1" without decimal
    // points. Keep using the component range to detect legacy byte colors.
    if (parsedColor.r > 1.0f || parsedColor.g > 1.0f || parsedColor.b > 1.0f
	|| (vectorSize == 4 && parsedColor.a > 1.0f)) {
	parsedColor /= 255.0f;
	if (vectorSize == 3) {
	    parsedColor.a = alpha;
	}
    }

    return Model::Color (parsedColor);
}
