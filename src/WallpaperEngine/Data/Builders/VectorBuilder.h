#pragma once

#include <algorithm>
#include <cstring>
#include <glm/detail/qualifier.hpp>
#include <glm/detail/type_vec1.hpp>
#include <string>
#include <vector>

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
    template <typename type> static type convert (const char* str);

    static constexpr const char* kSpaces = " \t\r\n";
    static constexpr const char* kSeparators = " \t\r\n,";

    /**
     * Splits str into its values and stores where each one starts.
     * Values are separated by whitespace, or by one comma with optional whitespace around it,
     * so "1 2", "1,2", "1, 2" and "1 , 2" all hold two values. Stops after 5 values, since
     * no vector holds more than 4.
     *
     * @param str
     * @param values Receives a pointer to the start of each value
     * @return false if the string or a value is empty, as in "", "1,,2", "1,2," or ",1"
     */
    static bool split (const std::string& str, std::vector<const char*>& values) {
	const char* p = str.c_str ();
	p += strspn (p, kSpaces);

	if (*p == '\0') {
	    return false;
	}

	while (values.size () <= 4) {
	    const size_t valueLength = strcspn (p, kSeparators);

	    if (valueLength == 0 && *p == ',') {
		return false;
	    }

	    values.push_back (p);
	    p += valueLength;
	    p += strspn (p, kSpaces);

	    if (*p == ',') {
		p++;
		p += strspn (p, kSpaces);

		if (*p == '\0' || *p == ',') {
		    return false;
		}
	    }

	    if (*p == '\0') {
		return true;
	    }
	}

	return true;
    }

public:
    /**
     * Takes the string and returns the vector size (2, 3 or 4), using the same
     * separators as parse. Five or more numbers count as 4, since a vec4 ignores the
     * extras. Returns 1 for anything that is not 2 or more numbers, so callers can
     * treat it as a scalar or a text value.
     *
     * TODO: THIS SHOULD BE MOVED, RENAMED OR PLACED SOMEWHERE WHERE IT MAKES MORE SENSE
     *
     * @param str
     * @return
     */
    static int preparseSize (const std::string& str) {
	std::vector<const char*> values;

	if (!split (str, values) || values.size () < 2) {
	    return 1;
	}

	for (const char* value : values) {
	    char* end = nullptr;
	    std::strtof (value, &end);

	    // the number must end at a separator or the end of the string (strchr matches '\0' too)
	    if (end == value || strchr (kSeparators, *end) == nullptr) {
		return 1;
	    }
	}

	// a vec4 ignores values past the fourth, as it always has (see parse)
	return std::min (static_cast<int> (values.size ()), 4);
    }

    /**
     * Takes a string value and parses it into a glm::vec.
     * Values are separated by spaces or commas (some workshop effects store colors as
     * "0.10,0.11,0.12"), and parsed with the basic std::strto* functions.
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

	std::vector<const char*> values;

	if (!split (str, values)) {
	    sLog.exception ("Invalid vector format: " + str + " (empty value)");
	}

	// validate lengths against what was found in the strings
	if (values.size () < static_cast<size_t> (length)) {
	    sLog.exception ("Invalid vector format: " + str + " (too few values, expected: ", length, ")");
	}

	// vec4 has always ignored values past the fourth, so only shorter vectors reject extras
	if (length < 4 && values.size () > static_cast<size_t> (length)) {
	    sLog.exception ("Invalid vector format: " + str + " (too many values, expected: ", length, ")");
	}

	// lengths validated, values can be used directly without issues
	if constexpr (length == 1) {
	    return glm::vec<length, type, qualifier> (convert<type> (values[0]));
	} else if constexpr (length == 2) {
	    return { convert<type> (values[0]), convert<type> (values[1]) };
	} else if constexpr (length == 3) {
	    return { convert<type> (values[0]), convert<type> (values[1]), convert<type> (values[2]) };
	} else if constexpr (length == 4) {
	    return { convert<type> (values[0]), convert<type> (values[1]), convert<type> (values[2]),
		     convert<type> (values[3]) };
	}
    }
    template <typename T, typename std::enable_if_t<is_glm_vec<T>::value, int> = 0>
    [[nodiscard]] static T parse (const std::string& str) {
	constexpr int length = GlmVecTraits<T>::length;
	constexpr glm::qualifier qualifier = GlmVecTraits<T>::qualifier;

	// call the specialized version of the function
	return parse<length, typename GlmVecTraits<T>::type, qualifier> (str);
    }
};

template <> inline float VectorBuilder::convert<float> (const char* str) { return std::strtof (str, nullptr); }

template <> inline int VectorBuilder::convert<int> (const char* str) { return std::stoi (str); }

template <> inline unsigned int VectorBuilder::convert<unsigned int> (const char* str) {
    return std::strtoul (str, nullptr, 10);
}

template <> inline double VectorBuilder::convert<double> (const char* str) { return std::strtod (str, nullptr); }

template <> inline uint8_t VectorBuilder::convert<uint8_t> (const char* str) { return std::strtoul (str, nullptr, 10); }

template <> inline uint16_t VectorBuilder::convert<uint16_t> (const char* str) {
    return std::strtoul (str, nullptr, 10);
}

template <> inline uint64_t VectorBuilder::convert<uint64_t> (const char* str) {
    return std::strtoull (str, nullptr, 10);
}

template <> inline int8_t VectorBuilder::convert<int8_t> (const char* str) { return std::strtol (str, nullptr, 10); }

template <> inline int16_t VectorBuilder::convert<int16_t> (const char* str) { return std::strtol (str, nullptr, 10); }

template <> inline int64_t VectorBuilder::convert<int64_t> (const char* str) { return std::strtoll (str, nullptr, 10); }

template <> inline bool VectorBuilder::convert<bool> (const char* str) { return std::strtoul (str, nullptr, 10) > 0; }

} // namespace WallpaperEngine::Data::Parsers
