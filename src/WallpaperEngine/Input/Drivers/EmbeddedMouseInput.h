#pragma once

#include "WallpaperEngine/Input/MouseInput.h"

namespace WallpaperEngine::Render::Drivers {
class EmbeddedHost;
}

namespace WallpaperEngine::Input::Drivers {
/**
 * Mouse input backed by the embedding host.
 *
 * The host reports the pointer with a top-left origin, matching the window
 * systems the other drivers deal with; update() converts it to the engine's
 * bottom-left convention. Clicks are always reported as released: a wallpaper
 * sits behind the desktop and must never consume button presses.
 */
class EmbeddedMouseInput final : public MouseInput {
public:
    explicit EmbeddedMouseInput (const Render::Drivers::EmbeddedHost& host);

    void update () override;
    [[nodiscard]] glm::dvec2 position () const override;
    [[nodiscard]] MouseClickStatus leftClick () const override;
    [[nodiscard]] MouseClickStatus rightClick () const override;

private:
    const Render::Drivers::EmbeddedHost& m_host;
    glm::dvec2 m_position = {0, 0};
};
} // namespace WallpaperEngine::Input::Drivers
