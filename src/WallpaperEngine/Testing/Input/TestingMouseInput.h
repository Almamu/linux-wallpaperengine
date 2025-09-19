#pragma once

#include "WallpaperEngine/Input/InputContext.h"

namespace WallpaperEngine::Testing::Input {
using namespace WallpaperEngine::Input;

class TestingMouseInput final : public MouseInput {
  public:
    void update () override;
    [[nodiscard]] glm::dvec2 position () const override;
    [[nodiscard]] MouseClickStatus leftClick () const override;
    [[nodiscard]] MouseClickStatus rightClick () const override;
};
} // namespace WallpaperEngine::Testing::Input
