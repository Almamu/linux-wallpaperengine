#pragma once

#include "CMouseInput.h"

namespace WallpaperEngine::Render::Drivers {
class CVideoDriver;
}

namespace WallpaperEngine::Input {
class CInputContext {
  public:
    explicit CInputContext (CMouseInput& mouseInput);

    /**
     * Updates input information
     */
    void update ();

    [[nodiscard]] const CMouseInput& getMouseInput () const;

  private:
    CMouseInput& m_mouse;
};
} // namespace WallpaperEngine::Input
