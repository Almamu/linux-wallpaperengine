#pragma once

#include "MouseInput.h"

namespace WallpaperEngine::Render::Drivers {
class VideoDriver;
}

namespace WallpaperEngine::Input {
class InputContext {
public:
    explicit InputContext (MouseInput& mouseInput);

    /**
     * Updates input information
     */
    void update ();

    [[nodiscard]] const MouseInput& getMouseInput () const;

private:
    MouseInput& m_mouse;
};
} // namespace WallpaperEngine::Input
