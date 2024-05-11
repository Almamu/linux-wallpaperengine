#pragma once

#include "COutputViewport.h"

namespace WallpaperEngine::Render::Drivers::Output {
class CGLFWOutputViewport final : public COutputViewport {
  public:
    CGLFWOutputViewport (glm::ivec4 viewport, std::string name);

    void makeCurrent () override;
    void swapOutput () override;
};
} // namespace WallpaperEngine::Render::Drivers::Output
