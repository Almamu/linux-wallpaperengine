#pragma once

#include <cstring>
#include <string>
#include <glm/detail/qualifier.hpp>
#include <glm/detail/type_vec1.hpp>

#include "WallpaperEngine/Logging/CLog.h"
#include "WallpaperEngine/Data/Utils/SFINAE.h"

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
    template <typename type>
    static type convert (const char* str);

  public:
    /**
     * Takes the string and returns the vector size (2, 3 or 4)
     *
     * TODO: THIS SHOULD BE MOVED, RENAMED OR PLACED SOMEWHERE WHERE IT MAKES MORE SENSE
     *
     * @param str
     * @return
     */
    static int preparseSize (const std::string& str) {
        const char* p = str.c_str ();
        const char* first = strchr (p, ' ');
        const char* second = first ? strchr (first + 1, ' ') : nullptr;
        const char* third = second ? strchr (second + 1, ' ') : nullptr;

        if (first == nullptr) {
            sLog.exception ("Invalid vector format: " + str + " (too few values, expected: 2, 3 or 4)");
        } else if (second == nullptr) {
            return 2;
        } else if (third == nullptr) {
            return 3;
        } else {
            return 4;
        }
    }

    /**
     * Takes a string value and parses it into a glm::vec.
     * This particular parsing uses spaces as separators and basic std::strto* functions
     * for the actual parsing of the values.
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
        const char* p = str.c_str ();

        // get up to 4 spaces
        const char* first = strchr (p, ' ');
        const char* second = first ? strchr (first + 1, ' ') : nullptr;
        const char* third = second ? strchr (second + 1, ' ') : nullptr;

        // validate lengths against what was found in the strings
        if constexpr (length == 1) {
            if (first != nullptr) {
                sLog.exception ("Invalid vector format: " + str + " (too many values, expected: ", length, ")");
            }
        } else if constexpr (length == 2) {
            if (first == nullptr) {
                sLog.exception ("Invalid vector format: " + str + " (too few values, expected: ", length, ")");
            } else if (second != nullptr) {
                sLog.exception ("Invalid vector format: " + str + " (too many values, expected: ", length, ")");
            }
        } else if constexpr (length == 3) {
            if (first == nullptr || second == nullptr) {
                sLog.exception ("Invalid vector format: " + str + " (too few values, expected: ", length, ")");
            } else if (third != nullptr) {
                sLog.exception ("Invalid vector format: " + str + " (too many values, expected: ", length, ")");
            }
        } else if constexpr (length == 4) {
            if (first == nullptr || second == nullptr || third == nullptr) {
                sLog.exception ("Invalid vector format: " + str + " (too few values, expected: ", length, ")");
            }
        } else {
            sLog.exception ("Invalid vector length: ", length);
        }

        // lengths validated, values can be used directly without issues
        if constexpr (length == 1) {
            return {
                convert <type> (p)
            };
        } else if constexpr (length == 2) {
            return {
                convert <type> (p),
                convert <type> (first + 1)
            };
        } else if constexpr (length == 3) {
            return {
                convert <type> (p),
                convert <type> (first + 1),
                convert <type> (second + 1)
            };
        } else {
            return {
                convert <type> (p),
                convert <type> (first + 1),
                convert <type> (second + 1),
                convert <type> (third + 1)
            };
        }
    }
    template <typename T, typename std::enable_if_t<is_glm_vec<T>::value, int> = 0>
    [[nodiscard]] static T parse (const std::string& str) {
        constexpr int length = GlmVecTraits<T>::length;
        constexpr glm::qualifier qualifier = GlmVecTraits<T>::qualifier;

        // call the specialized version of the function
        return parse<length, typename GlmVecTraits<T>::type, qualifier> (str);
    }
  private:
};

template <>
inline float VectorBuilder::convert<float> (const char* str) {
    return std::strtof (str, nullptr);
}

template <>
inline int VectorBuilder::convert<int> (const char* str) {
    return std::stoi (str);
}

template <>
inline unsigned int VectorBuilder::convert<unsigned int> (const char* str) {
    return std::strtoul (str, nullptr, 10);
}

template <>
inline double VectorBuilder::convert<double> (const char* str) {
    return std::strtod (str, nullptr);
}

template <>
inline uint8_t VectorBuilder::convert<uint8_t> (const char* str) {
    return std::strtoul (str, nullptr, 10);
}

template <>
inline uint16_t VectorBuilder::convert<uint16_t> (const char* str) {
    return std::strtoul (str, nullptr, 10);
}

template <>
inline uint64_t VectorBuilder::convert<uint64_t> (const char* str) {
    return std::strtoull (str, nullptr, 10);
}

template <>
inline int8_t VectorBuilder::convert<int8_t> (const char* str) {
    return std::strtol (str, nullptr, 10);
}

template <>
inline int16_t VectorBuilder::convert<int16_t> (const char* str) {
    return std::strtol (str, nullptr, 10);
}

template <>
inline int64_t VectorBuilder::convert<int64_t> (const char* str) {
    return std::strtoll (str, nullptr, 10);
}

template <>
inline bool VectorBuilder::convert<bool> (const char* str) {
    return std::strtoul (str, nullptr, 10) > 0;
}

} // namespace WallpaperEngine::Data::Parsers
