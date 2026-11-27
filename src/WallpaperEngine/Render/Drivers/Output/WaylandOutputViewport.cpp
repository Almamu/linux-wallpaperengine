#include "WaylandOutputViewport.h"
#include "WallpaperEngine/Logging/Log.h"
#include "color-management-v1-protocol.h"

#define class _class
#define namespace _namespace
#define static
extern "C" {
#include "wlr-layer-shell-unstable-v1-protocol.h"
#include "xdg-output-unstable-v1-protocol.h"
#include "xdg-shell-protocol.h"
}
#undef class
#undef namespace
#undef static

using namespace WallpaperEngine::Render::Drivers;
using namespace WallpaperEngine::Render::Drivers::Output;

static void handleLSConfigure (void* data, zwlr_layer_surface_v1* surface, uint32_t serial, uint32_t w, uint32_t h) {
    const auto viewport = static_cast<WaylandOutputViewport*> (data);
    viewport->size = { w, h };
    viewport->logicalSize = { w, h };
    viewport->viewport = { 0, 0, viewport->size.x * viewport->scale, viewport->size.y * viewport->scale };
    viewport->resize ();
    viewport->getDriver ()->onOutputChanged ();

    zwlr_layer_surface_v1_ack_configure (surface, serial);
}

static void handleLSClosed (void* data, zwlr_layer_surface_v1* surface) {
    const auto viewport = static_cast<WaylandOutputViewport*> (data);

    viewport->getDriver ()->onLayerClose (viewport);
}

static void geometry (
    void* data, wl_output* output, int32_t x, int32_t y, int32_t width_mm, int32_t height_mm, int32_t subpixel,
    const char* make, const char* model, int32_t transform
) {
    const auto viewport = static_cast<WaylandOutputViewport*> (data);
    // only use geometry position as fallback if xdg-output hasn't provided one
    if (!viewport->hasXdgLogicalPosition) {
	viewport->globalPosition = { x, y };
    }
    sLog.debug ("SPAN DEBUG geometry: output '", viewport->name, "' position=(", x, ",", y, ") transform=", transform);
}

static void mode (void* data, wl_output* output, uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
    const auto viewport = static_cast<WaylandOutputViewport*> (data);

    if (!(flags & WL_OUTPUT_MODE_CURRENT)) {
	return;
    }
    viewport->refreshRate = std::max (0, refresh);
    viewport->frameCredits = std::min (viewport->frameCredits, viewport->frameBudget ());

    // A mode event is in physical pixels. Keep the surface-local size selected by
    // configure when a refresh-rate change or compositor reload repeats the mode.
    if (viewport->layerSurface) {
	return;
    }

    viewport->size = { width, height };
    viewport->viewport = { 0, 0, viewport->size.x * viewport->scale, viewport->size.y * viewport->scale };

    if (viewport->initialized) {
	viewport->getDriver ()->getOutput ().reset ();
    }
}

static void done (void* data, wl_output* wl_output) {
    const auto viewport = static_cast<WaylandOutputViewport*> (data);
    viewport->initialized = true;
    viewport->setupAttempts = 0;
    viewport->getDriver ()->onOutputChanged ();
}

static void scale (void* data, wl_output* wl_output, int32_t scale) {
    const auto viewport = static_cast<WaylandOutputViewport*> (data);

    viewport->scale = scale;
    viewport->viewport = { 0, 0, viewport->size.x * viewport->scale, viewport->size.y * viewport->scale };

    if (viewport->layerSurface) {
	viewport->resize ();
    }

    if (viewport->initialized) {
	viewport->getDriver ()->getOutput ().reset ();
    }
}

static void name (void* data, wl_output* wl_output, const char* name) {
    const auto viewport = static_cast<WaylandOutputViewport*> (data);

    if (name) {
	viewport->name = name;
    }

    // ensure the output is updated with the new name too
    viewport->getDriver ()->getOutput ().reset ();
}

static void description (void* data, wl_output* wl_output, const char* description) {
    // ignored
}

static void surfaceFrameCallback (void* data, struct wl_callback* cb, uint32_t time) {
    const auto viewport = static_cast<WaylandOutputViewport*> (data);

    wl_callback_destroy (cb);

    viewport->frameCallback = nullptr;
    viewport->frameCredits = viewport->frameBudget ();
}

constexpr struct wl_callback_listener frameListener = { .done = surfaceFrameCallback };

constexpr wl_output_listener outputListener
    = { .geometry = geometry, .mode = mode, .done = done, .scale = scale, .name = name, .description = description };

constexpr struct zwlr_layer_surface_v1_listener layerSurfaceListener = {
    .configure = handleLSConfigure,
    .closed = handleLSClosed,
};

static void xdgOutputLogicalPosition (void* data, struct zxdg_output_v1* xdg_output, int32_t x, int32_t y) {
    const auto viewport = static_cast<WaylandOutputViewport*> (data);
    viewport->globalPosition = { x, y };
    viewport->hasXdgLogicalPosition = true;
    viewport->getDriver ()->onOutputChanged ();
    sLog.debug ("SPAN DEBUG xdg-output logical_position: '", viewport->name, "' position=(", x, ",", y, ")");
}

static void xdgOutputLogicalSize (void* data, struct zxdg_output_v1* xdg_output, int32_t width, int32_t height) {
    const auto viewport = static_cast<WaylandOutputViewport*> (data);
    viewport->logicalSize = { width, height };
    viewport->getDriver ()->onOutputChanged ();
    if (viewport->initialized) {
	viewport->getDriver ()->getOutput ().reset ();
    }
}

static void xdgOutputDone (void* data, struct zxdg_output_v1* xdg_output) {
    // deprecated since xdg-output v3, compositor uses wl_output.done instead
}

static void xdgOutputName (void* data, struct zxdg_output_v1* xdg_output, const char* outputName) {
    const auto viewport = static_cast<WaylandOutputViewport*> (data);
    if (outputName && viewport->output && wl_output_get_version (viewport->output) < WL_OUTPUT_NAME_SINCE_VERSION) {
	viewport->name = outputName;
	// Core v1 has no done event. The connector name is enough to match the requested output.
	if (wl_output_get_version (viewport->output) < WL_OUTPUT_DONE_SINCE_VERSION) {
	    viewport->initialized = true;
	}
	viewport->getDriver ()->onOutputChanged ();
    }
}

static void xdgOutputDescription (void* data, struct zxdg_output_v1* xdg_output, const char* description) {
    // ignored
}

constexpr struct zxdg_output_v1_listener xdgOutputListener = {
    .logical_position = xdgOutputLogicalPosition,
    .logical_size = xdgOutputLogicalSize,
    .done = xdgOutputDone,
    .name = xdgOutputName,
    .description = xdgOutputDescription,
};

WaylandOutputViewport::WaylandOutputViewport (
    WaylandOpenGLDriver* driver, uint32_t waylandName, struct wl_registry* registry, uint32_t version
) : OutputViewport ({ 0, 0, 0, 0 }, "", true), size ({ 0, 0 }), waylandName (waylandName), m_driver (driver) {
    // setup output listener
    this->output = static_cast<wl_output*> (wl_registry_bind (registry, waylandName, &wl_output_interface, std::min (version, 4u)));
    wl_output_add_listener (output, &outputListener, this);
}

void WaylandOutputViewport::setupXdgOutput (zxdg_output_manager_v1* manager) {
    this->xdgOutput = zxdg_output_manager_v1_get_xdg_output (manager, this->output);
    zxdg_output_v1_add_listener (this->xdgOutput, &xdgOutputListener, this);
}

void WaylandOutputViewport::setupLS (bool allowSDRFallback) {
    surface = wl_compositor_create_surface (m_driver->getWaylandContext ()->compositor);
    if (!surface) {
	sLog.exception ("Failed to create a Wayland surface");
    }

    zwlr_layer_shell_v1_layer wlrLayer;
    switch (m_driver->getApp ().getContext ().settings.render.wayland.layer) {
	case WallpaperEngine::Application::ApplicationContext::WAYLAND_LAYER_BACKGROUND:
	    wlrLayer = ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;
	    break;
	case WallpaperEngine::Application::ApplicationContext::WAYLAND_LAYER_TOP:
	    wlrLayer = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
	    break;
	case WallpaperEngine::Application::ApplicationContext::WAYLAND_LAYER_OVERLAY:
	    wlrLayer = ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY;
	    break;
	case WallpaperEngine::Application::ApplicationContext::WAYLAND_LAYER_BOTTOM:
	default:
	    wlrLayer = ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;
	    break;
    }

    layerSurface = zwlr_layer_shell_v1_get_layer_surface (
	m_driver->getWaylandContext ()->layerShell, surface, output, wlrLayer, "linux-wallpaperengine"
    );

    if (!layerSurface) {
	sLog.exception ("Failed to get a layer surface");
    }

    wl_region* region = wl_compositor_create_region (m_driver->getWaylandContext ()->compositor);
    if (m_driver->getApp ().getContext ().settings.mouse.enabled) {
	wl_region_add (region, 0, 0, INT32_MAX, INT32_MAX);
    }

    // Mark the surface as fully opaque so the compositor can skip rendering
    // anything below it and avoid alpha-blending. Wallpapers are by definition
    // the bottommost visible content, so this is always a win.
    wl_region* opaqueRegion = wl_compositor_create_region (m_driver->getWaylandContext ()->compositor);
    wl_region_add (opaqueRegion, 0, 0, INT32_MAX, INT32_MAX);
    wl_surface_set_opaque_region (surface, opaqueRegion);
    wl_region_destroy (opaqueRegion);

    zwlr_layer_surface_v1_set_size (layerSurface, 0, 0);
    zwlr_layer_surface_v1_set_anchor (
	layerSurface,
	ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
	    | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
    );
    zwlr_layer_surface_v1_set_keyboard_interactivity (layerSurface, false);
    zwlr_layer_surface_v1_add_listener (layerSurface, &layerSurfaceListener, this);
    zwlr_layer_surface_v1_set_exclusive_zone (layerSurface, -1);
    wl_surface_set_input_region (surface, region);
    wl_region_destroy (region);
    wl_surface_commit (surface);
    if (wl_display_roundtrip (m_driver->getWaylandContext ()->display) < 0) {
	sLog.exception ("Wayland connection failed while configuring output ", name);
    }
    if (!layerSurface) {
	return; // The compositor closed or removed this output during the roundtrip.
    }

    eglWindow = wl_egl_window_create (surface, size.x * scale, size.y * scale);
    if (!eglWindow) {
	sLog.exception ("Failed to create an EGL window for output ", name);
    }
    eglSurface = m_driver->getEGLContext ()->eglCreatePlatformWindowSurfaceEXT (
	m_driver->getEGLContext ()->display, m_driver->getEGLContext ()->config, eglWindow, nullptr
    );
    if (eglSurface == EGL_NO_SURFACE && (!allowSDRFallback || !m_driver->fallbackToSDR ()))
        sLog.exception ("Failed to create an EGL window surface");
    m_colorSurface = m_driver->getWaylandContext ()->colorManagement.getSurface (surface);
    wl_surface_commit (surface);
    if (wl_display_roundtrip (m_driver->getWaylandContext ()->display) < 0) {
	sLog.exception ("Wayland connection failed while setting up output ", name);
    }
    if (!layerSurface) {
	return;
    }
    wl_display_flush (m_driver->getWaylandContext ()->display);

    static const auto XCURSORSIZE = getenv ("XCURSOR_SIZE") ? std::stoi (getenv ("XCURSOR_SIZE")) : 24;
    m_cursorTheme
	= wl_cursor_theme_load (getenv ("XCURSOR_THEME"), XCURSORSIZE * scale, m_driver->getWaylandContext ()->shm);

    if (!m_cursorTheme) {
	sLog.exception ("Failed to get a cursor theme");
    }

    pointer = wl_cursor_theme_get_cursor (m_cursorTheme, "left_ptr");
    cursorSurface = wl_compositor_create_surface (m_driver->getWaylandContext ()->compositor);

    if (!cursorSurface) {
	sLog.exception ("Failed to get a cursor surface");
    }

    if (eglMakeCurrent (
            m_driver->getEGLContext ()->display, eglSurface, eglSurface, m_driver->getEGLContext ()->context
        ) == EGL_FALSE) {
        if (!allowSDRFallback || !m_driver->fallbackToSDR () || eglMakeCurrent (
                m_driver->getEGLContext ()->display, eglSurface, eglSurface, m_driver->getEGLContext ()->context
            ) == EGL_FALSE) sLog.exception ("Failed to make egl current");
    }

    // Surface frame callbacks already pace each output. A blocking swap on a
    // slower monitor would also stall every other scene on the shared thread.
    if (eglSwapInterval (m_driver->getEGLContext ()->display, 0) == EGL_FALSE) {
	sLog.error ("Couldn't disable blocking EGL swaps for output ", name);
    }

    this->m_driver->getOutput ().reset ();
}

void WaylandOutputViewport::destroyLS () {
    if (frameCallback) wl_callback_destroy (frameCallback);
    frameCallback = nullptr;

    if (eglSurface != EGL_NO_SURFACE) {
	eglDestroySurface (m_driver->getEGLContext ()->display, eglSurface);
    }
    eglSurface = EGL_NO_SURFACE;
    if (eglWindow) wl_egl_window_destroy (eglWindow);
    eglWindow = nullptr;

    if (m_colorSurface) wp_color_management_surface_v1_destroy (m_colorSurface);
    m_colorSurface = nullptr;
    m_scRGBPresentation = false;
    if (layerSurface) zwlr_layer_surface_v1_destroy (layerSurface);
    layerSurface = nullptr;
    if (surface) wl_surface_destroy (surface);
    surface = nullptr;
    if (cursorSurface) wl_surface_destroy (cursorSurface);
    cursorSurface = nullptr;
    if (m_cursorTheme) wl_cursor_theme_destroy (m_cursorTheme);
    m_cursorTheme = nullptr;
    pointer = nullptr;

    leftClick = WallpaperEngine::Input::MouseClickStatus::Released;
    rightClick = WallpaperEngine::Input::MouseClickStatus::Released;
    hasMousePosition = false;
    callbackInitialized = false;
    frameCredits = 1;
    nextFrameTime = {};
    lastRenderDuration = {};
}

WaylandOpenGLDriver* WaylandOutputViewport::getDriver () const { return this->m_driver; }

void WaylandOutputViewport::makeCurrent () {
    const EGLBoolean result = eglMakeCurrent (
	m_driver->getEGLContext ()->display, eglSurface, eglSurface, m_driver->getEGLContext ()->context
    );

    if (result == EGL_FALSE) {
	sLog.error ("Couldn't make egl current");
    }
}

void WaylandOutputViewport::swapOutput () {
    this->callbackInitialized = true;

    this->makeCurrent ();
    // A redraw can arrive before the compositor completes the previous frame.
    // Keep that callback: overwriting it loses the handle and queues duplicate
    // render callbacks. surfaceFrameCallback clears it when the frame is ready.
    if (frameCallback == nullptr) {
	frameCallback = wl_surface_frame (surface);
	wl_callback_add_listener (frameCallback, &frameListener, this);
    }
    // EGL commits the new buffer. Apply its scale and damage in that same
    // commit instead of issuing a second, bufferless commit after the swap.
    wl_surface_set_buffer_scale (surface, scale);
    wl_surface_damage_buffer (surface, 0, 0, INT32_MAX, INT32_MAX);
    m_driver->getWaylandContext ()->colorManagement.queuePresentation (
        m_colorSurface, m_scRGBPresentation && supportsScRGB ()
    );
    eglSwapBuffers (m_driver->getEGLContext ()->display, this->eglSurface);
}

bool WaylandOutputViewport::supportsScRGB () const {
    return surface && eglWindow && eglSurface != EGL_NO_SURFACE && m_colorSurface
        && m_driver->getEGLContext ()->properties.isScRGB ()
        && m_driver->getWaylandContext ()->colorManagement.ready ();
}

void WaylandOutputViewport::setScRGBPresentation (bool enabled) { m_scRGBPresentation = enabled; }

int WaylandOutputViewport::frameBudget () const {
    // Below the output refresh rate, allow one frame to be prepared while its
    // callback is in flight. If callbacks stop, this finite budget runs out.
    // Unknown/slow modes retain callback pacing; mode refresh is not an FPS cap.
    const auto fps = std::max (1, m_driver->getApp ().getContext ().settings.render.maximumFPS);
    return static_cast<int64_t> (refreshRate) > static_cast<int64_t> (fps) * 1000 ? 2 : 1;
}

void WaylandOutputViewport::resize () {
    if (!this->eglWindow) {
	return;
    }

    wl_egl_window_resize (this->eglWindow, this->size.x * this->scale, this->size.y * this->scale, 0, 0);

    this->getDriver ()->getOutput ().reset ();
}
