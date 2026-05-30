#pragma once
#include <glm/vec4.hpp>

namespace WallpaperEngine::Data::Model {
/**
 * Representation of a color using RGBA components in the 0.0f - 1.0f range
 */
struct Color : private glm::vec4 {
    using glm::vec4::vec4;
    using glm::vec4::operator=;
    explicit Color (const glm::vec4& v) : glm::vec4 (v) {}
    using glm::vec4::r;
    using glm::vec4::g;
    using glm::vec4::b;
    using glm::vec4::a;
};
}