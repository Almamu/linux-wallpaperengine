#include "CWaylandOutputViewport.h"
#include "WallpaperEngine/Application/CWallpaperApplication.h"
#include "WallpaperEngine/Logging/CLog.h"

#define class _class
#define namespace _namespace
#define static
extern "C" {
#include "wlr-layer-shell-unstable-v1-protocol.h"
#include "xdg-shell-protocol.h"
}
#undef class
#undef namespace
#undef static

#include <unistd.h>

using namespace WallpaperEngine::Render::Drivers;
using namespace WallpaperEngine::Render::Drivers::Output;

static void handleLSConfigure (void* data, zwlr_layer_surface_v1* surface, uint32_t serial, uint32_t w, uint32_t h) {
    const auto viewport = static_cast<CWaylandOutputViewport*> (data);
    viewport->size = {w, h};
    viewport->viewport = {0, 0, viewport->size.x * viewport->scale, viewport->size.y * viewport->scale};
    viewport->resize ();

    zwlr_layer_surface_v1_ack_configure (surface, serial);
}

static void handleLSClosed (void* data, zwlr_layer_surface_v1* surface) {
    const auto viewport = static_cast<CWaylandOutputViewport*> (data);

    viewport->getDriver ()->onLayerClose (viewport);
}

static void geometry (void* data, wl_output* output, int32_t x, int32_t y, int32_t width_mm, int32_t height_mm,
                      int32_t subpixel, const char* make, const char* model, int32_t transform) {
    // ignored
}

static void mode (void* data, wl_output* output, uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
    const auto viewport = static_cast<CWaylandOutputViewport*> (data);

    // update viewport size too
    viewport->size = {width, height};
    viewport->viewport = {0, 0, viewport->size.x * viewport->scale, viewport->size.y * viewport->scale};

    if (viewport->layerSurface)
        viewport->resize ();

    if (viewport->initialized)
        viewport->getDriver ()->getOutput ().reset ();
}

static void done (void* data, wl_output* wl_output) {
    static_cast<CWaylandOutputViewport*> (data)->initialized = true;
}

static void scale (void* data, wl_output* wl_output, int32_t scale) {
    const auto viewport = static_cast<CWaylandOutputViewport*> (data);

    viewport->scale = scale;

    if (viewport->layerSurface)
        viewport->resize ();

    if (viewport->initialized)
        viewport->getDriver ()->getOutput ().reset ();
}

static void name (void* data, wl_output* wl_output, const char* name) {
    const auto viewport = static_cast<CWaylandOutputViewport*> (data);

    if (name)
        viewport->name = name;

    // ensure the output is updated with the new name too
    viewport->getDriver ()->getOutput ().reset ();
}

static void description (void* data, wl_output* wl_output, const char* description) {
    // ignored
}

static void surfaceFrameCallback (void* data, struct wl_callback* cb, uint32_t time) {
    const auto viewport = static_cast<CWaylandOutputViewport*> (data);

    wl_callback_destroy (cb);

    viewport->frameCallback = nullptr;
    viewport->rendering = true;
    viewport->getDriver ()->getApp ().update (viewport);
    viewport->rendering = false;
}

constexpr struct wl_callback_listener frameListener = {.done = surfaceFrameCallback};

constexpr wl_output_listener outputListener = {
    .geometry = geometry, .mode = mode, .done = done, .scale = scale, .name = name, .description = description};

constexpr struct zwlr_layer_surface_v1_listener layerSurfaceListener = {
    .configure = handleLSConfigure,
    .closed = handleLSClosed,
};

CWaylandOutputViewport::CWaylandOutputViewport (CWaylandOpenGLDriver* driver, uint32_t waylandName,
                                                struct wl_registry* registry) :
    m_driver (driver),
    waylandName (waylandName),
    COutputViewport ({0, 0, 0, 0}, "", true) {
    // setup output listener
    this->output = static_cast<wl_output*> (wl_registry_bind (registry, waylandName, &wl_output_interface, 4));
    this->name = "";
    this->size = {0, 0};
    wl_output_add_listener (output, &outputListener, this);
}

void CWaylandOutputViewport::setupLS () {
    surface = wl_compositor_create_surface (m_driver->getWaylandContext ()->compositor);
    layerSurface =
        zwlr_layer_shell_v1_get_layer_surface (m_driver->getWaylandContext ()->layerShell, surface, output,
                                               ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "linux-wallpaperengine");

    if (!layerSurface)
        sLog.exception ("Failed to get a layer surface");

    wl_region* region = wl_compositor_create_region (m_driver->getWaylandContext ()->compositor);
    wl_region_add (region, 0, 0, INT32_MAX, INT32_MAX);

    zwlr_layer_surface_v1_set_size (layerSurface, 0, 0);
    zwlr_layer_surface_v1_set_anchor (layerSurface,
                                      ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                                          ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM);
    zwlr_layer_surface_v1_set_keyboard_interactivity (layerSurface, false);
    zwlr_layer_surface_v1_add_listener (layerSurface, &layerSurfaceListener, this);
    zwlr_layer_surface_v1_set_exclusive_zone (layerSurface, -1);
    wl_surface_set_input_region (surface, region);
    wl_surface_commit (surface);
    wl_display_roundtrip (m_driver->getWaylandContext ()->display);

    eglWindow = wl_egl_window_create (surface, size.x * scale, size.y * scale);
    eglSurface = m_driver->getEGLContext ()->eglCreatePlatformWindowSurfaceEXT (
        m_driver->getEGLContext ()->display, m_driver->getEGLContext ()->config, eglWindow, nullptr);
    wl_surface_commit (surface);
    wl_display_roundtrip (m_driver->getWaylandContext ()->display);
    wl_display_flush (m_driver->getWaylandContext ()->display);

    static const auto XCURSORSIZE = getenv ("XCURSOR_SIZE") ? std::stoi (getenv ("XCURSOR_SIZE")) : 24;
    const auto PRCURSORTHEME =
        wl_cursor_theme_load (getenv ("XCURSOR_THEME"), XCURSORSIZE * scale, m_driver->getWaylandContext ()->shm);

    if (!PRCURSORTHEME)
        sLog.exception ("Failed to get a cursor theme");

    pointer = wl_cursor_theme_get_cursor (PRCURSORTHEME, "left_ptr");
    cursorSurface = wl_compositor_create_surface (m_driver->getWaylandContext ()->compositor);

    if (!cursorSurface)
        sLog.exception ("Failed to get a cursor surface");

    if (eglMakeCurrent (m_driver->getEGLContext ()->display, eglSurface, eglSurface,
                        m_driver->getEGLContext ()->context) == EGL_FALSE)
        sLog.exception ("Failed to make egl current");

    this->m_driver->getOutput ().reset ();
}

CWaylandOpenGLDriver* CWaylandOutputViewport::getDriver () {
    return this->m_driver;
}

void CWaylandOutputViewport::makeCurrent () {
    const EGLBoolean result = eglMakeCurrent (m_driver->getEGLContext ()->display, eglSurface, eglSurface,
                                              m_driver->getEGLContext ()->context);

    if (result == EGL_FALSE)
        sLog.error ("Couldn't make egl current");
}

void CWaylandOutputViewport::swapOutput () {
    this->callbackInitialized = true;

    this->makeCurrent ();
    frameCallback = wl_surface_frame (surface);
    wl_callback_add_listener (frameCallback, &frameListener, this);
    eglSwapBuffers (m_driver->getEGLContext ()->display, this->eglSurface);
    wl_surface_set_buffer_scale (surface, scale);
    wl_surface_damage_buffer (surface, 0, 0, INT32_MAX, INT32_MAX);
    wl_surface_commit (surface);
}

void CWaylandOutputViewport::resize () {
    if (!this->eglWindow)
        return;

    wl_egl_window_resize (this->eglWindow, this->size.x * this->scale, this->size.y * this->scale, 0, 0);

    this->getDriver ()->getOutput ().reset ();
}