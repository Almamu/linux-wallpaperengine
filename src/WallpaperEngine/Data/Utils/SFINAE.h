#pragma once

#include <glm/detail/qualifier.hpp>
#include <glm/detail/type_vec1.hpp>

namespace WallpaperEngine::Data::Utils::SFINAE {
// sfinae to detect the type for template specialization
template <typename T> struct is_glm_vec : std::false_type { };

template <glm::length_t L, typename S, glm::qualifier Q> struct is_glm_vec<glm::vec<L, S, Q>> : std::true_type { };

// traits used to guess the type of the vector and it's length
template <typename T> struct GlmVecTraits;

template <glm::length_t L, typename T, glm::qualifier Q> struct GlmVecTraits<glm::vec<L, T, Q>> {
    static constexpr int length = L;
    using type = T;
    static constexpr glm::qualifier qualifier = Q;
};

}