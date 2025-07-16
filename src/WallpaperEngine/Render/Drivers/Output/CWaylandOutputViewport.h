#pragma once

#ifdef ENABLE_WAYLAND

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glew.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <wayland-egl.h>

#include "../CWaylandOpenGLDriver.h"
#include <WallpaperEngine/Input/CMouseInput.h>
#include "COutputViewport.h"
#include <glm/vec2.hpp>

struct zwlr_layer_shell_v1;
struct zwlr_layer_surface_v1;

namespace WallpaperEngine::Render::Drivers {
class CWaylandOpenGLDriver;

namespace Output {
class COutputViewport;

class CWaylandOutputViewport final : public COutputViewport {
  public:
    CWaylandOutputViewport (CWaylandOpenGLDriver* driver, uint32_t waylandName, struct wl_registry* registry);

    /**
     * @return The wayland driver
     */
    CWaylandOpenGLDriver* getDriver ();

    wl_output* output = nullptr;
    glm::ivec2 size = {};
    uint32_t waylandName;
    int scale = 1;
    bool initialized = false;
    bool rendering = false;

    wl_egl_window* eglWindow = nullptr;
    EGLSurface eglSurface = nullptr;
    wl_surface* surface = nullptr;
    zwlr_layer_surface_v1* layerSurface = nullptr;
    wl_callback* frameCallback = nullptr;
    glm::dvec2 mousePos = {0, 0};
    WallpaperEngine::Input::MouseClickStatus leftClick = WallpaperEngine::Input::MouseClickStatus::Released;
    WallpaperEngine::Input::MouseClickStatus rightClick = WallpaperEngine::Input::MouseClickStatus::Released;
    wl_cursor* pointer = nullptr;
    wl_surface* cursorSurface = nullptr;
    bool callbackInitialized = false;

    void setupLS ();

    /**
     * Activates output's context for drawing
     */
    void makeCurrent () override;

    /**
     * Swaps buffers to present data on the viewport
     */
    void swapOutput () override;

    /**
     * Updates the viewport size
     */
    void resize ();

  private:
    CWaylandOpenGLDriver* m_driver = nullptr;
};
} // namespace Output
} // namespace WallpaperEngine::Render::Drivers
#endif /* ENABLE_WAYLAND */