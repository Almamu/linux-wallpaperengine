#include "WaylandOpenGLDriver.h"
#include "VideoFactories.h"
#include "WallpaperEngine/Application/WallpaperApplication.h"
#include "WallpaperEngine/Logging/Log.h"

#define class _class
#define namespace _namespace
#define static
extern "C" {
#include "wlr-layer-shell-unstable-v1-protocol.h"
#include "xdg-output-unstable-v1-protocol.h"
#include "xdg-shell-protocol.h"
#include <linux/input-event-codes.h>
}
#undef class
#undef namespace
#undef static

#include "color-management-v1-protocol.h"

#include <algorithm>
#include <cerrno>
#include <poll.h>
#include <string.h>
#include <utility>

using namespace WallpaperEngine::Render::Drivers;

extern float g_Time;

static void handlePointerEnter (
    void* data, struct wl_pointer* wl_pointer, uint32_t serial, struct wl_surface* surface, wl_fixed_t surface_x,
    wl_fixed_t surface_y
) {
    const auto driver = static_cast<WaylandOpenGLDriver*> (data);
    const auto viewport = driver->surfaceToViewport (surface);
    driver->viewportInFocus = viewport;
    if (!viewport) {
	return;
    }
    viewport->mousePos = {
	wl_fixed_to_double (surface_x) * viewport->scale,
	(viewport->size.y - wl_fixed_to_double (surface_y)) * viewport->scale,
    };
    viewport->hasMousePosition = true;
    if (!viewport->cursorSurface || !viewport->pointer) {
	return;
    }
    wl_surface_set_buffer_scale (viewport->cursorSurface, viewport->scale);
    wl_surface_attach (viewport->cursorSurface, wl_cursor_image_get_buffer (viewport->pointer->images[0]), 0, 0);
    wl_pointer_set_cursor (
	wl_pointer, serial, viewport->cursorSurface, viewport->pointer->images[0]->hotspot_x,
	viewport->pointer->images[0]->hotspot_y
    );
    wl_surface_commit (viewport->cursorSurface);
}

static void
handlePointerLeave (void* data, struct wl_pointer* wl_pointer, uint32_t serial, struct wl_surface* surface) {
    const auto driver = static_cast<WaylandOpenGLDriver*> (data);
    if (driver->viewportInFocus && driver->viewportInFocus->surface == surface) {
	driver->viewportInFocus->leftClick = WallpaperEngine::Input::MouseClickStatus::Released;
	driver->viewportInFocus->rightClick = WallpaperEngine::Input::MouseClickStatus::Released;
	driver->viewportInFocus = nullptr;
    }
}

static void handlePointerAxis (void* data, wl_pointer* wl_pointer, uint32_t time, uint32_t axis, wl_fixed_t value) { }

static void handlePointerMotion (
    void* data, struct wl_pointer* wl_pointer, uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y
) {
    const auto driver = static_cast<WaylandOpenGLDriver*> (data);

    const auto x = wl_fixed_to_double (surface_x);
    auto y = wl_fixed_to_double (surface_y);

    if (!driver->viewportInFocus) {
	return;
    }

    // Convert from Wayland coordinate system (Y=0 at top) to OpenGL coordinate system (Y=0 at bottom)
    const double viewportHeight = static_cast<double> (driver->viewportInFocus->size.y);
    y = viewportHeight - y;

    driver->viewportInFocus->mousePos = { x * driver->viewportInFocus->scale, y * driver->viewportInFocus->scale };
    driver->viewportInFocus->hasMousePosition = true;
}

static void handlePointerButton (
    void* data, struct wl_pointer* wl_pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t button_state
) {
    const auto driver = static_cast<WaylandOpenGLDriver*> (data);

    if (!driver->viewportInFocus) {
	return;
    }

    if (button == BTN_LEFT) {
	if (button_state == WL_POINTER_BUTTON_STATE_PRESSED) {
	    driver->viewportInFocus->leftClick = WallpaperEngine::Input::MouseClickStatus::Clicked;
	} else if (button_state == WL_POINTER_BUTTON_STATE_RELEASED) {
	    driver->viewportInFocus->leftClick = WallpaperEngine::Input::MouseClickStatus::Released;
	}
    } else if (button == BTN_RIGHT) {
	if (button_state == WL_POINTER_BUTTON_STATE_PRESSED) {
	    driver->viewportInFocus->rightClick = WallpaperEngine::Input::MouseClickStatus::Clicked;
	} else if (button_state == WL_POINTER_BUTTON_STATE_RELEASED) {
	    driver->viewportInFocus->rightClick = WallpaperEngine::Input::MouseClickStatus::Released;
	}
    }
}

constexpr struct wl_pointer_listener pointerListener = { .enter = handlePointerEnter,
							 .leave = handlePointerLeave,
							 .motion = handlePointerMotion,
							 .button = handlePointerButton,
							 .axis = handlePointerAxis };

static void handleCapabilities (void* data, wl_seat* wl_seat, uint32_t capabilities) {
    if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
	wl_pointer_add_listener (wl_seat_get_pointer (wl_seat), &pointerListener, data);
    }
}

constexpr struct wl_seat_listener seatListener = { .capabilities = handleCapabilities };

static void
handleGlobal (void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
    const auto driver = static_cast<WaylandOpenGLDriver*> (data);

    if (strcmp (interface, wl_compositor_interface.name) == 0) {
	driver->getWaylandContext ()->compositor
	    = static_cast<wl_compositor*> (wl_registry_bind (registry, name, &wl_compositor_interface, 4));
    } else if (strcmp (interface, wl_shm_interface.name) == 0) {
	driver->getWaylandContext ()->shm
	    = static_cast<wl_shm*> (wl_registry_bind (registry, name, &wl_shm_interface, 1));
    } else if (strcmp (interface, wl_output_interface.name) == 0) {
	driver->onOutputAdded (name, version);
    } else if (strcmp (interface, zwlr_layer_shell_v1_interface.name) == 0) {
	driver->getWaylandContext ()->layerShell
	    = static_cast<zwlr_layer_shell_v1*> (wl_registry_bind (registry, name, &zwlr_layer_shell_v1_interface, 1));
    } else if (strcmp (interface, wl_seat_interface.name) == 0) {
	driver->getWaylandContext ()->seat
	    = static_cast<wl_seat*> (wl_registry_bind (registry, name, &wl_seat_interface, 1));
	wl_seat_add_listener (driver->getWaylandContext ()->seat, &seatListener, driver);
    } else if (strcmp (interface, wp_color_manager_v1_interface.name) == 0) {
	driver->getWaylandContext ()->colorManagement.bind (registry, name, version);
    } else if (strcmp (interface, zxdg_output_manager_v1_interface.name) == 0) {
	driver->getWaylandContext ()->xdgOutputManager = static_cast<zxdg_output_manager_v1*> (
	    wl_registry_bind (registry, name, &zxdg_output_manager_v1_interface, std::min (version, 3u))
	);
	driver->onXdgOutputManagerBound ();
    }
}

static void handleGlobalRemoved (void* data, struct wl_registry* registry, uint32_t id) {
    const auto driver = static_cast<WaylandOpenGLDriver*> (data);
    driver->getWaylandContext ()->colorManagement.removeGlobal (id);
    driver->onOutputRemoved (id);
}

constexpr struct wl_registry_listener registryListener = {
    .global = handleGlobal,
    .global_remove = handleGlobalRemoved,
};

void WaylandOpenGLDriver::initEGL () {
    const char* CLIENT_EXTENSIONS = eglQueryString (EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (!CLIENT_EXTENSIONS) {
	sLog.exception ("Failed to query EGL Extensions");
    }

    const auto CLIENTEXTENSIONS = std::string (CLIENT_EXTENSIONS);

    if (CLIENTEXTENSIONS.find ("EGL_EXT_platform_base") == std::string::npos) {
	sLog.exception ("EGL_EXT_platform_base not supported by EGL!");
    }

    if (CLIENTEXTENSIONS.find ("EGL_EXT_platform_wayland") == std::string::npos) {
	sLog.exception ("EGL_EXT_platform_wayland not supported by EGL!");
    }

    const auto eglGetPlatformDisplayEXT
	= reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC> (eglGetProcAddress ("eglGetPlatformDisplayEXT"));
    m_eglContext.eglCreatePlatformWindowSurfaceEXT = reinterpret_cast<PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC> (
	eglGetProcAddress ("eglCreatePlatformWindowSurfaceEXT")
    );

    if (!eglGetPlatformDisplayEXT || !m_eglContext.eglCreatePlatformWindowSurfaceEXT) {
	sLog.exception ("EGL did not return EXT proc pointers!");
    }

    m_eglContext.display = eglGetPlatformDisplayEXT (EGL_PLATFORM_WAYLAND_EXT, m_waylandContext.display, nullptr);

    if (m_eglContext.display == EGL_NO_DISPLAY) {
	this->finishEGL ();
	sLog.exception ("eglGetPlatformDisplayEXT failed!");
    }

    if (!eglInitialize (m_eglContext.display, nullptr, nullptr)) {
	this->finishEGL ();
	sLog.exception ("eglInitialize failed!");
    }

    const auto CLIENTEXTENSIONSPOSTINIT = std::string (eglQueryString (m_eglContext.display, EGL_EXTENSIONS));

    if (CLIENTEXTENSIONSPOSTINIT.find ("EGL_KHR_create_context") == std::string::npos) {
	this->finishEGL ();
	sLog.exception ("EGL_KHR_create_context not supported!");
    }

    EGLint matchedConfigs = 0;
    const EGLint CONFIG_ATTRIBUTES[] = {
	EGL_SURFACE_TYPE,    EGL_WINDOW_BIT, EGL_RED_SIZE, 1, EGL_GREEN_SIZE, 1, EGL_BLUE_SIZE, 1, EGL_SAMPLES, 4,
	EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_NONE,
    };

    if (!eglChooseConfig (m_eglContext.display, CONFIG_ATTRIBUTES, &m_eglContext.config, 1, &matchedConfigs)) {
	this->finishEGL ();
	sLog.exception ("eglChooseConfig failed!");
    }

    if (matchedConfigs == 0) {
	this->finishEGL ();
	sLog.exception ("eglChooseConfig failed! (matched 0 configs)");
    }

    m_eglContext.sdrConfig = m_eglContext.config;
    queryWaylandEGLConfig (m_eglContext.display, m_eglContext.config, m_eglContext.properties);
    if (m_waylandContext.colorManagement.hasScRGBProtocol ()) {
        if (const auto config = chooseWaylandFloatConfig (m_eglContext.display, m_eglContext.properties))
            m_eglContext.config = config;
    }

    if (!eglBindAPI (EGL_OPENGL_API)) {
	this->finishEGL ();
	sLog.exception ("eglBindAPI failed!");
    }

    m_eglContext.context = createEGLContext ();
    if (m_eglContext.context == EGL_NO_CONTEXT && m_eglContext.properties.isScRGB ()) fallbackToSDR ();
    if (m_eglContext.context == EGL_NO_CONTEXT) {
        this->finishEGL ();
        sLog.error ("eglCreateContext error " + std::to_string (eglGetError ()));
        sLog.exception ("eglCreateContext failed!");
    }
    sLog.debug ("Wayland EGL config: RGBA=", m_eglContext.properties.red, "/", m_eglContext.properties.green,
                "/", m_eglContext.properties.blue, "/", m_eglContext.properties.alpha,
                " component=", m_eglContext.properties.componentType, " samples=", m_eglContext.properties.samples);
}

EGLContext WaylandOpenGLDriver::createEGLContext () {
    const EGLint CONTEXT_ATTRIBUTES[] = {
	EGL_CONTEXT_MAJOR_VERSION_KHR,
	3,
	EGL_CONTEXT_MINOR_VERSION_KHR,
	3,
	EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,
	EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
#if !NDEBUG
	// debug context so KHR_debug messages carry the driver's explanation for each GL error;
	// NVIDIA generates no messages at all on non-debug contexts
	EGL_CONTEXT_FLAGS_KHR,
	EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR,
#endif
	EGL_NONE,
    };

    return eglCreateContext (m_eglContext.display, m_eglContext.config, EGL_NO_CONTEXT, CONTEXT_ATTRIBUTES);
}

bool WaylandOpenGLDriver::fallbackToSDR () {
    if (!m_eglContext.properties.isScRGB ()) return false;
    // All outputs are still in setup: no wallpaper resources exist yet. Rebuild
    // their surfaces together so the shared context never mixes incompatible configs.
    eglMakeCurrent (m_eglContext.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    for (const auto& screen : m_screens) {
        if (screen->eglSurface != EGL_NO_SURFACE) eglDestroySurface (m_eglContext.display, screen->eglSurface);
        screen->eglSurface = EGL_NO_SURFACE;
    }
    if (m_eglContext.context != EGL_NO_CONTEXT) eglDestroyContext (m_eglContext.display, m_eglContext.context);
    m_eglContext.config = m_eglContext.sdrConfig;
    m_eglContext.properties = {};
    queryWaylandEGLConfig (m_eglContext.display, m_eglContext.config, m_eglContext.properties);
    m_eglContext.context = createEGLContext ();
    if (m_eglContext.context == EGL_NO_CONTEXT) return false;
    for (const auto& screen : m_screens) {
        if (!screen->eglWindow) continue;
        screen->eglSurface = m_eglContext.eglCreatePlatformWindowSurfaceEXT (
            m_eglContext.display, m_eglContext.config, screen->eglWindow, nullptr
        );
        if (screen->eglSurface == EGL_NO_SURFACE || !eglMakeCurrent (
                m_eglContext.display, screen->eglSurface, screen->eglSurface, m_eglContext.context
            )) return false;
        if (!eglSwapInterval (m_eglContext.display, 0))
            sLog.error ("Couldn't disable blocking EGL swaps for output ", screen->name);
    }
    sLog.debug ("Wayland EGL reverted to SDR: RGBA=", m_eglContext.properties.red, "/", m_eglContext.properties.green,
                "/", m_eglContext.properties.blue, "/", m_eglContext.properties.alpha,
                " component=", m_eglContext.properties.componentType, " samples=", m_eglContext.properties.samples);
    return true;
}

bool WaylandOpenGLDriver::makeBuildContextCurrent () {
    // runs on the async wallpaper build worker thread; EGL "current" state is per-thread,
    // so this never disturbs the render thread's context
    if (!eglBindAPI (EGL_OPENGL_API)) {
	sLog.error ("build context: eglBindAPI failed");
	return false;
    }

    if (this->m_buildContext == EGL_NO_CONTEXT) {
	const EGLint CONTEXT_ATTRIBUTES[] = {
	    EGL_CONTEXT_MAJOR_VERSION_KHR,
	    3,
	    EGL_CONTEXT_MINOR_VERSION_KHR,
	    3,
	    EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,
	    EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
#if !NDEBUG
	    EGL_CONTEXT_FLAGS_KHR,
	    EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR,
#endif
	    EGL_NONE,
	};

	// share objects (textures, buffers, shaders) with the render context; container
	// objects (FBOs, VAOs) are not shared and are created lazily on the render thread
	this->m_buildContext = eglCreateContext (
	    this->m_eglContext.display, this->m_eglContext.config, this->m_eglContext.context, CONTEXT_ATTRIBUTES
	);

	if (this->m_buildContext == EGL_NO_CONTEXT) {
	    sLog.error ("build context: eglCreateContext error ", eglGetError ());
	    return false;
	}
    }

    // surfaceless is supported basically everywhere mesa runs; try it first
    if (eglMakeCurrent (this->m_eglContext.display, EGL_NO_SURFACE, EGL_NO_SURFACE, this->m_buildContext)) {
	return true;
    }

    // fall back to a 1x1 pbuffer for drivers without EGL_KHR_surfaceless_context
    if (this->m_buildSurface == EGL_NO_SURFACE) {
	EGLint matchedConfigs = 0;
	EGLConfig pbufferConfig = nullptr;
	const EGLint CONFIG_ATTRIBUTES[] = {
	    EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_NONE,
	};
	const EGLint PBUFFER_ATTRIBUTES[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };

	if (!eglChooseConfig (this->m_eglContext.display, CONFIG_ATTRIBUTES, &pbufferConfig, 1, &matchedConfigs)
	    || matchedConfigs == 0) {
	    sLog.error ("build context: no pbuffer-capable EGLConfig available");
	    return false;
	}

	this->m_buildSurface = eglCreatePbufferSurface (this->m_eglContext.display, pbufferConfig, PBUFFER_ATTRIBUTES);

	if (this->m_buildSurface == EGL_NO_SURFACE) {
	    sLog.error ("build context: eglCreatePbufferSurface error ", eglGetError ());
	    return false;
	}
    }

    if (!eglMakeCurrent (
	    this->m_eglContext.display, this->m_buildSurface, this->m_buildSurface, this->m_buildContext
	)) {
	sLog.error ("build context: eglMakeCurrent error ", eglGetError ());
	return false;
    }

    return true;
}

void WaylandOpenGLDriver::releaseBuildContext () {
    // unbind from the worker thread; the context itself is destroyed with the display
    eglMakeCurrent (this->m_eglContext.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglReleaseThread ();
}

void WaylandOpenGLDriver::finishEGL () const {
    eglMakeCurrent (EGL_NO_DISPLAY, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (m_eglContext.display) {
	eglTerminate (m_eglContext.display);
    }
    eglReleaseThread ();
}

void WaylandOpenGLDriver::onLayerClose (Output::WaylandOutputViewport* viewport) {
    sLog.out ("Compositor closed the wallpaper surface for ", viewport->name);
    releaseLayerSurface (viewport);
    getOutput ().reset ();
    onOutputChanged ();
}

void WaylandOpenGLDriver::onOutputAdded (uint32_t name, uint32_t version) {
    auto* viewport = new Output::WaylandOutputViewport (this, name, m_waylandContext.registry, version);
    m_screens.push_back (viewport);
    if (m_waylandContext.xdgOutputManager) {
	viewport->setupXdgOutput (m_waylandContext.xdgOutputManager);
    }
}

void WaylandOpenGLDriver::onXdgOutputManagerBound () {
    for (auto* viewport : m_screens) {
	if (viewport->output && !viewport->xdgOutput) {
	    viewport->setupXdgOutput (m_waylandContext.xdgOutputManager);
	}
    }
}

void WaylandOpenGLDriver::onOutputRemoved (uint32_t name) {
    const auto found = std::ranges::find_if (m_screens, [name] (const auto* viewport) {
	return viewport->waylandName == name && !viewport->removed;
    });
    if (found == m_screens.end ()) {
	return;
    }
    auto* viewport = *found;
    sLog.out ("Output ", viewport->name, " removed");
    // A setup roundtrip may deliver removal of the output currently being set up.
    // Release its objects now and keep the viewport alive until that call returns.
    viewport->removed = true;
    releaseOutput (viewport);
    getOutput ().reset ();
    onOutputChanged ();
}

void WaylandOpenGLDriver::releaseLayerSurface (Output::WaylandOutputViewport* viewport) {
    if (viewportInFocus == viewport) {
	viewportInFocus = nullptr;
    }
    if (viewport->eglSurface != EGL_NO_SURFACE && eglGetCurrentSurface (EGL_DRAW) == viewport->eglSurface) {
	bool current = false;
	for (const auto* other : m_screens) {
	    if (other != viewport && other->eglSurface != EGL_NO_SURFACE
		&& eglMakeCurrent (m_eglContext.display, other->eglSurface, other->eglSurface, m_eglContext.context)) {
		current = true;
		break;
	    }
	}
	// Keep the render context available for cleanup and non-presentation GL work
	// while all outputs are absent, on drivers that support a surfaceless context.
	if (!current && !eglMakeCurrent (m_eglContext.display, EGL_NO_SURFACE, EGL_NO_SURFACE, m_eglContext.context)) {
	    eglMakeCurrent (m_eglContext.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	}
    }
    viewport->destroyLS ();
}

void WaylandOpenGLDriver::releaseOutput (Output::WaylandOutputViewport* viewport) {
    releaseLayerSurface (viewport);
    if (viewport->xdgOutput) {
	zxdg_output_v1_destroy (viewport->xdgOutput);
	viewport->xdgOutput = nullptr;
    }
    if (viewport->output) {
	if (wl_output_get_version (viewport->output) >= WL_OUTPUT_RELEASE_SINCE_VERSION) {
	    wl_output_release (viewport->output);
	} else {
	    wl_output_destroy (viewport->output);
	}
	viewport->output = nullptr;
    }
}

void WaylandOpenGLDriver::removeDisconnectedOutputs () {
    std::erase_if (m_screens, [] (auto* viewport) {
	if (!viewport->removed) {
	    return false;
	}
	delete viewport;
	return true;
    });
}

void WaylandOpenGLDriver::processOutputChanges () {
    if (!std::exchange (m_outputChangePending, false)) {
	return;
    }
    constexpr int maxAttempts = 3;
    const auto screens = m_screens; // setup can dispatch further additions and removals
    for (auto* viewport : screens) {
	if (viewport->removed || viewport->layerSurface || !viewport->initialized || viewport->setupAttempts >= maxAttempts
	    || !isOutputRequested (viewport->name)) {
	    continue;
	}
	++viewport->setupAttempts;
	try {
	    // The EGL context already owns wallpaper resources. Only startup can replace it with SDR.
	    viewport->setupLS (false);
	} catch (const std::exception&) {
	    releaseLayerSurface (viewport);
	    getOutput ().reset ();
	}
	if (wl_display_get_error (m_waylandContext.display) != 0) {
	    m_requestedExit = m_abnormalExit = true;
	    break;
	}
	if (viewport->layerSurface) {
	    viewport->setupAttempts = 0;
	} else if (!viewport->removed && viewport->setupAttempts < maxAttempts) {
	    onOutputChanged ();
	}
    }
    removeDisconnectedOutputs ();
    if (!m_requestedExit) {
	try {
	    getApp ().refreshSpanGeometry ();
	} catch (const std::exception& error) {
	    sLog.error ("Cannot prepare a newly connected span group: ", error.what ());
	}
    }
}

WaylandOpenGLDriver::WaylandOpenGLDriver (ApplicationContext& context, WallpaperApplication& app) :
    VideoDriver (app, m_mouseInput), m_output (context, *this), m_requestedExit (false), m_frameCounter (0),
    m_context (context), m_mouseInput (*this) {
    initWaylandRegistry ();
    initEGL ();
    setupOutputLayerSurfaces ();
    initGLEW ();
}

void WaylandOpenGLDriver::initWaylandRegistry () {
    m_waylandContext.display = wl_display_connect (nullptr);

    if (!m_waylandContext.display) {
	sLog.exception ("Failed to query wayland display");
    }

    m_waylandContext.registry = wl_display_get_registry (m_waylandContext.display);
    wl_registry_add_listener (m_waylandContext.registry, &registryListener, this);

    wl_display_dispatch (m_waylandContext.display);
    wl_display_roundtrip (m_waylandContext.display);

    if (!m_waylandContext.compositor || !m_waylandContext.shm || !m_waylandContext.layerShell
	|| this->m_screens.empty ()) {
	sLog.exception ("Failed to bind to required interfaces");
    }

    if (!m_waylandContext.xdgOutputManager && !m_context.settings.general.spanGroups.empty ()) {
	sLog.error ("zxdg_output_manager_v1 is unavailable; screen-span positions will be incorrect.");
    }
    // Also receive the names of outputs bound during the first roundtrip.
    wl_display_roundtrip (m_waylandContext.display);
}

bool WaylandOpenGLDriver::isOutputRequested (const std::string& name) const {
    if (name.empty ()) {
	return false;
    }
    if (m_context.settings.general.screenBackgrounds.contains (name)) {
	return true;
    }
    return std::ranges::any_of (m_context.settings.general.spanGroups, [&name] (const auto& group) {
	return std::ranges::find (group.screens, name) != group.screens.end ();
    });
}

void WaylandOpenGLDriver::setupOutputLayerSurfaces () {
    const auto screens = m_screens; // setup can dispatch further additions and removals
    for (auto* viewport : screens) {
	if (!viewport->removed && isOutputRequested (viewport->name)) {
	    viewport->setupLS (true);
	}
    }
    removeDisconnectedOutputs ();

    if (getOutput ().getViewports ().empty ()) {
	sLog.error ("No outputs could be initialized, please check the parameters and try again");
	sLog.error ("Detected outputs:");

	for (const auto& o : this->m_screens) {
	    sLog.error ("  ", o->name);
	}

	sLog.error ("Requested: ");

	for (const auto& o : m_context.settings.general.screenBackgrounds | std::views::keys) {
	    sLog.error ("  ", o);
	}

	for (const auto& spanGroup : m_context.settings.general.spanGroups) {
	    for (const auto& screen : spanGroup.screens) {
		sLog.error ("  ", screen, " (span group)");
	    }
	}

	sLog.exception ("Cannot continue...");
    }
}

void WaylandOpenGLDriver::initGLEW () {
    glewExperimental = GL_TRUE;
    if (const GLenum result = glewInit (); result != GLEW_OK) {
	if (result == GLEW_ERROR_NO_GLX_DISPLAY) {
	    // expected on Wayland (no GLX display), GL pointers are already loaded via EGL
	    sLog.debug ("GLEW GLX init skipped (Wayland/EGL context)");
	} else {
	    const char* error = reinterpret_cast<const char*> (glewGetErrorString (result));
	    sLog.error ("Failed to initialize GLEW: ", error ? error : "Unknown error");
	    sLog.exception ("Cannot continue...");
	}
    }
}

WaylandOpenGLDriver::~WaylandOpenGLDriver () {
    const auto screens = std::exchange (m_screens, {});
    for (auto* viewport : screens) {
	releaseOutput (viewport);
	delete viewport;
    }

    // stop EGL
    eglMakeCurrent (EGL_NO_DISPLAY, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    if (m_buildSurface != EGL_NO_SURFACE) {
	eglDestroySurface (m_eglContext.display, m_buildSurface);
    }

    if (m_buildContext != EGL_NO_CONTEXT) {
	eglDestroyContext (m_eglContext.display, m_buildContext);
    }

    if (m_eglContext.context != EGL_NO_CONTEXT) {
	eglDestroyContext (m_eglContext.display, m_eglContext.context);
    }

    eglTerminate (m_eglContext.display);
    eglReleaseThread ();

    m_waylandContext.colorManagement.reset ();

    // disconnect from wayland display
    if (this->m_waylandContext.display) {
	wl_display_disconnect (this->m_waylandContext.display);
    }
}

void WaylandOpenGLDriver::dispatchEventQueue () { dispatchEvents (true); }

void WaylandOpenGLDriver::dispatchPendingEvents () { dispatchEvents (false); }

void WaylandOpenGLDriver::dispatchEvents (bool renderFrames) {
    using Clock = std::chrono::steady_clock;
    const auto frameInterval = [] (int fps) {
	return std::max (Clock::duration (1), std::chrono::duration_cast<Clock::duration> (
	    std::chrono::duration<double> (1.0 / fps)
	));
    };
    const int desktopFPS = std::max (1, this->m_context.settings.render.maximumFPS);
    const int streamFPS = this->getApp ().getLockscreenStreamFPS ();
    const auto presentationInterval = frameInterval (desktopFPS);
    const auto generationInterval = frameInterval (std::max (desktopFPS, streamFPS));
    const auto advanceDeadline = [] (Clock::time_point previous, Clock::time_point now, Clock::duration interval) {
	if (previous == Clock::time_point {}) return now + interval;
	// Keep the phase across late wakes and skip missed intervals after a stall.
	return previous + interval * ((now - previous) / interval + 1);
    };

    const auto pacingOutput = [&] {
	Output::WaylandOutputViewport* first = nullptr;
	for (const auto& viewport : this->getOutput ().getViewports () | std::views::values) {
	    auto* output = static_cast<Output::WaylandOutputViewport*> (viewport);
	    if (output->frameCredits > 0 && output->frameBudget () > 1
		&& (!first || output->nextFrameTime < first->nextFrameTime)) {
		first = output;
	    }
	}
	return first;
    };
    const auto deadline = [&] (
	const Output::WaylandOutputViewport* output, const Output::WaylandOutputViewport* first,
	Clock::time_point firstDeadline, Clock::time_point now
    ) {
	// Avoid starting a slower output just before a faster output is due.
	// Defer only when both measured draws fit in one interval, and only
	// until that fixed deadline; the slower output still runs in this batch.
	if (first && output->frameBudget () == 1 && firstDeadline > now
	    && firstDeadline - now < output->lastRenderDuration
	    && output->lastRenderDuration + first->lastRenderDuration < presentationInterval) {
	    return std::max (output->nextFrameTime, firstDeadline);
	}
	return output->nextFrameTime;
    };

    // A hidden or powered-off output may stop sending frame callbacks indefinitely.
    // Keep the application loop ticking so IPC and prepared switches still run.
    // Follow Wayland's prepare/read/cancel protocol: dispatch pending callbacks
    // before preparing a read, and never leave a prepared read outstanding.
    auto* display = m_waylandContext.display;
    while (wl_display_prepare_read (display) != 0) {
	if (wl_display_dispatch_pending (display) < 0) {
	    m_requestedExit = m_abnormalExit = true;
	    return;
	}
    }

    const int flushed = wl_display_flush (display);
    if (flushed < 0 && errno != EAGAIN) {
	wl_display_cancel_read (display);
	m_requestedExit = m_abnormalExit = true;
	return;
    }
    pollfd events { wl_display_get_fd (display), static_cast<short> (POLLIN | (flushed < 0 ? POLLOUT : 0)), 0 };
    // Bookkeeping events must not spend a frame's FPS budget. Wait for events
    // or the earliest ready output's deadline, with a bounded idle heartbeat
    // for IPC/CEF even when all outputs are hidden or the FPS cap is very low.
    auto wait = renderFrames ? std::chrono::duration_cast<Clock::duration> (std::chrono::milliseconds (16))
			     : Clock::duration::zero ();
    const auto now = Clock::now ();
    const auto* waitFirst = pacingOutput ();
    const auto waitFirstDeadline = waitFirst ? waitFirst->nextFrameTime : Clock::time_point {};
    for (const auto& viewport : this->getOutput ().getViewports () | std::views::values) {
	const auto* output = static_cast<Output::WaylandOutputViewport*> (viewport);
	const auto streamDeadline = this->getApp ().getLockscreenFrameDeadline (output->name);
	if (streamDeadline != Clock::time_point::max ()) {
	    wait = std::min (wait, std::max (Clock::duration::zero (), streamDeadline - now));
	}
	if (output->frameCredits > 0) {
	    wait = std::min (
		wait, std::max (Clock::duration::zero (), deadline (output, waitFirst, waitFirstDeadline, now) - now)
	    );
	}
    }
    const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds> (wait).count ();
    const timespec timeout { nanoseconds / 1000000000, nanoseconds % 1000000000 };
    const auto waitStart = Clock::now ();
    const int ready = ppoll (&events, 1, &timeout, nullptr);
    const int pollError = errno;
    m_lastFrameWait = wait > Clock::duration::zero () ? Clock::now () - waitStart : Clock::duration::zero ();
    if (ready > 0 && (events.revents & POLLIN)) {
	if (wl_display_read_events (display) < 0) {
	    m_requestedExit = m_abnormalExit = true;
	}
    } else {
	wl_display_cancel_read (display);
    }
    if ((ready < 0 && pollError != EINTR) || (events.revents & (POLLERR | POLLHUP | POLLNVAL))) {
	m_requestedExit = m_abnormalExit = true;
    }
    if (events.revents & POLLOUT) {
	if (wl_display_flush (display) < 0 && errno != EAGAIN) {
	    m_requestedExit = m_abnormalExit = true;
	}
    }
    if (!m_requestedExit && wl_display_dispatch_pending (display) < 0) {
	m_requestedExit = m_abnormalExit = true;
    }

    if (!m_requestedExit) {
	processOutputChanges ();
    }
    if (m_requestedExit || !renderFrames) {
	return;
    }

    auto* first = pacingOutput ();
    const auto firstDeadline = first ? first->nextFrameTime : Clock::time_point {};
    bool rendered = false;
    const auto renderOutput = [&] (Output::WaylandOutputViewport* output) {
	const auto frameStart = Clock::now ();
	const bool present = output->frameCredits > 0
	    && frameStart >= deadline (output, first, firstDeadline, frameStart);
	if (!present && frameStart < this->getApp ().getLockscreenFrameDeadline (output->name)) {
	    return;
	}
	const bool newFrame = frameStart >= m_nextFrameToken;
	if (!rendered || newFrame) {
	    // Polling can outlive the application's time sample, and a slow draw
	    // can carry the next output into another frame interval.
	    g_Time = this->getRenderTime ();
	}
	if (newFrame) {
	    // Reuse shared scenes/textures across staggered output callbacks in
	    // the same interval. Empty event wakes never consume screenshot frames.
	    m_frameCounter++;
	    m_nextFrameToken = advanceDeadline (m_nextFrameToken, frameStart, generationInterval);
	}
	if (present) {
	    --output->frameCredits;
	    // A faster feed may advance the scene before the display is due.
	    // Keep each output's presentation cap through changes in the lease.
	    const auto nextPresentation = advanceDeadline (output->nextFrameTime, frameStart, presentationInterval);
	    output->nextFrameTime = streamFPS > desktopFPS
		? nextPresentation
		: std::max (m_nextFrameToken, nextPresentation);
	}
	output->rendering = true;
	// A session lock can suppress callbacks indefinitely. The leased feed uses
	// an offscreen target and never swaps a hidden surface or fabricates credits.
	this->getApp ().update (output, present);
	if (present) output->lastRenderDuration = Clock::now () - frameStart;
	output->rendering = false;
	rendered = true;
    };
    // Connector names need not sort in refresh/deadline order.
    if (first) {
	renderOutput (first);
    }
    for (const auto& viewport : this->getOutput ().getViewports () | std::views::values) {
	auto* output = static_cast<Output::WaylandOutputViewport*> (viewport);
	if (output != first) {
	    renderOutput (output);
	}
    }
}

Output::Output& WaylandOpenGLDriver::getOutput () { return this->m_output; }

double WaylandOpenGLDriver::getRenderTime () const {
    return static_cast<double> (std::chrono::duration_cast<std::chrono::microseconds> (
				   std::chrono::high_resolution_clock::now () - renderStart
	   )
				   .count ())
	/ 1000000.0;
}

bool WaylandOpenGLDriver::closeRequested () { return this->m_requestedExit; }

bool WaylandOpenGLDriver::abnormalTermination () const { return this->m_abnormalExit; }

void WaylandOpenGLDriver::resizeWindow (glm::ivec2 size) { }

void WaylandOpenGLDriver::resizeWindow (glm::ivec4 sizeandpos) { }

void WaylandOpenGLDriver::showWindow () { }

void WaylandOpenGLDriver::hideWindow () { }

glm::ivec2 WaylandOpenGLDriver::getFramebufferSize () const { return glm::ivec2 { 0, 0 }; }

uint32_t WaylandOpenGLDriver::getFrameCounter () const { return m_frameCounter; }

std::chrono::steady_clock::duration WaylandOpenGLDriver::getLastFrameWait () const { return m_lastFrameWait; }

WaylandOpenGLDriver::SEGLContext* WaylandOpenGLDriver::getEGLContext () { return &this->m_eglContext; }

void* WaylandOpenGLDriver::getProcAddress (const char* name) const {
    return reinterpret_cast<void*> (eglGetProcAddress (name));
}

WaylandOpenGLDriver::WaylandContext* WaylandOpenGLDriver::getWaylandContext () { return &this->m_waylandContext; }

Output::WaylandOutputViewport* WaylandOpenGLDriver::surfaceToViewport (const wl_surface* surface) const {
    if (!surface) {
	return nullptr;
    }
    for (const auto& o : m_screens) {
	if (o->surface == surface) {
	    return o;
	}
    }

    return nullptr;
}

__attribute__ ((constructor)) void registerWaylandOpenGL () {
    sVideoFactories.registerDriver (
	ApplicationContext::DESKTOP_BACKGROUND, "wayland",
	[] (ApplicationContext& context, WallpaperApplication& application) -> std::unique_ptr<VideoDriver> {
	    return std::make_unique<WaylandOpenGLDriver> (context, application);
	}
    );
}
