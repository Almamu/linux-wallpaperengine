#include "WaylandMouseInput.h"
#include "WallpaperEngine/Render/Drivers/WaylandOpenGLDriver.h"
#include <glm/common.hpp>

using namespace WallpaperEngine::Input::Drivers;

namespace {
const WallpaperEngine::Render::Drivers::Output::WaylandOutputViewport* getActiveViewport (
    const WallpaperEngine::Render::Drivers::WaylandOpenGLDriver& driver
) {
    if (driver.viewportInFocus && driver.viewportInFocus->rendering) {
	return driver.viewportInFocus;
    }

    for (const auto* viewport : driver.m_screens) {
	if (viewport && viewport->rendering) {
	    return viewport;
	}
    }

    return nullptr;
}
}

WaylandMouseInput::WaylandMouseInput (const WallpaperEngine::Render::Drivers::WaylandOpenGLDriver& driver) :
    m_waylandDriver (driver) { }

void WaylandMouseInput::update () { }

glm::dvec2 WaylandMouseInput::position () const {
    if (!this->m_waylandDriver.getApp ().getContext ().settings.mouse.enabled) {
	return { 0, 0 };
    }

    const auto* viewport = getActiveViewport (m_waylandDriver);
    if (!viewport) {
	return { 0, 0 };
    }

    if (viewport == m_waylandDriver.viewportInFocus) {
	return viewport->mousePos;
    }

    if (viewport->mousePos.x != 0 || viewport->mousePos.y != 0) {
	return viewport->mousePos;
    }

    return {
	static_cast<double> (viewport->size.x * viewport->scale) / 2.0,
	static_cast<double> (viewport->size.y * viewport->scale) / 2.0,
    };
}

WallpaperEngine::Input::MouseClickStatus WaylandMouseInput::leftClick () const {
    const auto* viewport = getActiveViewport (m_waylandDriver);
    if (viewport) {
	return viewport->leftClick;
    }

    return MouseClickStatus::Released;
}

WallpaperEngine::Input::MouseClickStatus WaylandMouseInput::rightClick () const {
    const auto* viewport = getActiveViewport (m_waylandDriver);
    if (viewport) {
	return viewport->rightClick;
    }

    return MouseClickStatus::Released;
}