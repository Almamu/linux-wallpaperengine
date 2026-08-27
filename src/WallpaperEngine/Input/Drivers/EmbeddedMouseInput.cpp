#include "EmbeddedMouseInput.h"
#include "WallpaperEngine/Render/Drivers/EmbeddedHost.h"

using namespace WallpaperEngine::Input::Drivers;

EmbeddedMouseInput::EmbeddedMouseInput (const Render::Drivers::EmbeddedHost& host) : m_host (host) { }

void EmbeddedMouseInput::update () {
    const glm::dvec2 pointer = this->m_host.getPointerPosition ();

    // A negative position means the pointer is not over the wallpaper. Hold the
    // last known value instead of snapping to the origin, which would otherwise
    // yank parallax to a corner every time the cursor leaves the desktop.
    if (pointer.x < 0 || pointer.y < 0) {
	return;
    }

    // Host reports top-left origin, the engine works bottom-left.
    this->m_position = {pointer.x, static_cast<double> (this->m_host.getFramebufferSize ().y) - pointer.y};
}

glm::dvec2 EmbeddedMouseInput::position () const { return this->m_position; }

WallpaperEngine::Input::MouseClickStatus EmbeddedMouseInput::leftClick () const { return Released; }

WallpaperEngine::Input::MouseClickStatus EmbeddedMouseInput::rightClick () const { return Released; }
