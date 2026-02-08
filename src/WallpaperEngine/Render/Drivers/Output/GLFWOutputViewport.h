#pragma once

#include "OutputViewport.h"

namespace WallpaperEngine::Render::Drivers::Output {
class GLFWOutputViewport final : public OutputViewport {
public:
    GLFWOutputViewport (glm::ivec4 viewport, std::string name);

    void makeCurrent () override;
    void swapOutput () override;
};
} // namespace WallpaperEngine::Render::Drivers::Output
