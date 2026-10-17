#pragma once

#include <glm/vec4.hpp>

namespace WallpaperEngine::Render {
/** Information needed to render one wallpaper across multiple viewports. */
struct SpanInfo {
    /** Bounding box of the span group (x, y, width, height) in global desktop coordinates. */
    glm::ivec4 totalBounds;
};
} // namespace WallpaperEngine::Render
