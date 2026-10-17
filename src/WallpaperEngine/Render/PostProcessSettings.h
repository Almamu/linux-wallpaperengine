#pragma once

#include <glm/vec3.hpp>

namespace WallpaperEngine::Render {
struct PostProcessSettings {
    float contrast = 1.0f;
    float saturation = 1.0f;
    glm::vec3 borderColour = { 0.0f, 0.0f, 0.0f };
};
} // namespace WallpaperEngine::Render
