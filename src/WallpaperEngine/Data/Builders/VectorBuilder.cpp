#include "VectorBuilder.h"

#include <algorithm>

using namespace WallpaperEngine::Data::Builders;

VectorBuilder::Components VectorBuilder::split (const std::string& str) {
    constexpr std::string_view whitespace = " \t\r\n\f\v";
    constexpr std::string_view separators = " \t\r\n\f\v,";
    const std::string_view text (str);
    auto start = text.find_first_not_of (whitespace);
    Components components {};

    while (start != std::string_view::npos) {
	const auto end = text.find_first_of (separators, start);
	if (end == start) {
	    throw std::invalid_argument ("Empty vector component in " + str);
	}
	components.values [components.count++] = text.substr (start, end - start);
	if (end == std::string_view::npos || components.count == static_cast<int> (components.values.size ())) {
	    break;
	}
	start = text.find_first_not_of (whitespace, end);
	if (start != std::string_view::npos && text [start] == ',') {
	    start = text.find_first_not_of (whitespace, start + 1);
	    if (start == std::string_view::npos) {
		throw std::invalid_argument ("Empty vector component in " + str);
	    }
	}
    }

    return components;
}

int VectorBuilder::componentCount (const std::string& str) { return std::min (split (str).count, 4); }
