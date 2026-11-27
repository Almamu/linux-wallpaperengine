#pragma once

#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <glm/detail/qualifier.hpp>
#include <glm/detail/type_vec1.hpp>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

#include "WallpaperEngine/Data/Utils/SFINAE.h"
#include "WallpaperEngine/Logging/Log.h"

namespace WallpaperEngine::Data::Builders {
using namespace WallpaperEngine::Data::Utils::SFINAE;

class VectorBuilder {
    /**
     * Convert template that calls the proper std::strto* function
     * based on the incoming type
     *
     * @tparam type
     * @param str
     * @return
     */
    template <typename type> static type convert (const char* str, char** end);

    struct Components {
	std::array<std::string_view, 5> values;
	int count = 0;
    };

    static Components split (const std::string& str);

public:
    /**
     * Legacy space-based size hint for untyped values. A number containing a
     * comma, such as a formatted label, must not be inferred as a vector.
     */
    static int preparseSize (const std::string& str) {
	const char* p = str.c_str ();
	const char* first = strchr (p, ' ');
	const char* second = first ? strchr (first + 1, ' ') : nullptr;
	const char* third = second ? strchr (second + 1, ' ') : nullptr;

	if (first == nullptr) {
	    return 1;
	}

	if (second == nullptr) {
	    return 2;
	}

	if (third == nullptr) {
	    return 3;
	}

	return 4;
    }

    /** Counts typed components, up to four, with the same separators as parse. */
    static int componentCount (const std::string& str);

    /**
     * Takes a string value and parses it into a glm::vec.
     * Values can be separated by whitespace or commas.
     *
     * @tparam length Vector length
     * @tparam type Vector storage type
     * @tparam qualifier Precision qualifier
     *
     * @param str The string to parse the vector from
     *
     * @return
     */
    template <int length, typename type, glm::qualifier qualifier>
    [[nodiscard]] static glm::vec<length, type, qualifier> parse (const std::string& str) {
	// ensure a valid type is used, only 1 to 4 vectors are supported
	static_assert (length >= 1 && length <= 4, "Invalid vector length");

	const auto components = split (str);
	// Keep vec4's existing truncation of extra components.
	if (components.count < length || (length < 4 && components.count > length)) {
	    throw std::invalid_argument (
		"Invalid vector format: " + str + " (expected " + std::to_string (length) + " values)"
	    );
	}

	glm::vec<length, type, qualifier> value {};
	for (int i = 0; i < length; ++i) {
	    const auto component = components.values [i];
	    char* end = nullptr;
	    value [i] = convert<type> (component.data (), &end);
	    if (end != component.data () + component.size ()) {
		throw std::invalid_argument ("Invalid vector component in " + str);
	    }
	}
	return value;
    }
    template <typename T, typename std::enable_if_t<is_glm_vec<T>::value, int> = 0>
    [[nodiscard]] static T parse (const std::string& str) {
	constexpr int length = GlmVecTraits<T>::length;
	constexpr glm::qualifier qualifier = GlmVecTraits<T>::qualifier;

	// call the specialized version of the function
	return parse<length, typename GlmVecTraits<T>::type, qualifier> (str);
    }
};

template <> inline float VectorBuilder::convert<float> (const char* str, char** end) { return std::strtof (str, end); }

template <> inline int VectorBuilder::convert<int> (const char* str, char** end) {
    errno = 0;
    const auto value = std::strtol (str, end, 10);
    if (errno == ERANGE || value < std::numeric_limits<int>::min () || value > std::numeric_limits<int>::max ()) {
	throw std::out_of_range ("Vector integer out of range");
    }
    return static_cast<int> (value);
}

template <> inline unsigned int VectorBuilder::convert<unsigned int> (const char* str, char** end) {
    return std::strtoul (str, end, 10);
}

template <> inline double VectorBuilder::convert<double> (const char* str, char** end) { return std::strtod (str, end); }

template <> inline uint8_t VectorBuilder::convert<uint8_t> (const char* str, char** end) { return std::strtoul (str, end, 10); }

template <> inline uint16_t VectorBuilder::convert<uint16_t> (const char* str, char** end) {
    return std::strtoul (str, end, 10);
}

template <> inline uint64_t VectorBuilder::convert<uint64_t> (const char* str, char** end) {
    return std::strtoull (str, end, 10);
}

template <> inline int8_t VectorBuilder::convert<int8_t> (const char* str, char** end) { return std::strtol (str, end, 10); }

template <> inline int16_t VectorBuilder::convert<int16_t> (const char* str, char** end) { return std::strtol (str, end, 10); }

template <> inline int64_t VectorBuilder::convert<int64_t> (const char* str, char** end) { return std::strtoll (str, end, 10); }

template <> inline bool VectorBuilder::convert<bool> (const char* str, char** end) { return std::strtoul (str, end, 10) > 0; }

} // namespace WallpaperEngine::Data::Builders
