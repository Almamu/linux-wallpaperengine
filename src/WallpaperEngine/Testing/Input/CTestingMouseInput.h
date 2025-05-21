#pragma once

#include "WallpaperEngine/Input/CInputContext.h"

namespace WallpaperEngine::Testing::Input {
using namespace WallpaperEngine::Input;

class CTestingMouseInput final : public CMouseInput {
  public:
    void update () override;
    [[nodiscard]] glm::dvec2 position () const override;
    [[nodiscard]] MouseClickStatus leftClick () const override;
    [[nodiscard]] MouseClickStatus rightClick () const override;
};
} // namespace WallpaperEngine::Testing::Input
