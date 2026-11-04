#include "WaylandOpenGLDriver.h"
#include "Output/WaylandOutputSelection.h"
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

#include <algorithm>
#include <poll.h>
#include <string.h>
#include <unistd.h>
#include <utility>

using namespace WallpaperEngine::Render::Drivers;

static void handlePointerEnter (
    void* data, struct wl_pointer* wl_pointer, uint32_t serial, struct wl_surface* surface, wl_fixed_t surface_x,
    wl_fixed_t surface_y
) {
    const auto driver = static_cast<WaylandOpenGLDriver*> (data);
    const auto viewport = driver->surfaceToViewport (surface);
    driver->viewportInFocus = viewport;

    if (!viewport || !viewport->cursorSurface || !viewport->pointer) {
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
handlePointerLeave (void* data, struct wl_pointer* wl_pointer, uint32_t serial, struct wl_surface* surface) { }

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
	driver->onOutputAdded (name);
    } else if (strcmp (interface, zwlr_layer_shell_v1_interface.name) == 0) {
	driver->getWaylandContext ()->layerShell
	    = static_cast<zwlr_layer_shell_v1*> (wl_registry_bind (registry, name, &zwlr_layer_shell_v1_interface, 1));
    } else if (strcmp (interface, wl_seat_interface.name) == 0) {
	driver->getWaylandContext ()->seat
	    = static_cast<wl_seat*> (wl_registry_bind (registry, name, &wl_seat_interface, 1));
	wl_seat_add_listener (driver->getWaylandContext ()->seat, &seatListener, driver);
    } else if (strcmp (interface, zxdg_output_manager_v1_interface.name) == 0) {
	driver->getWaylandContext ()->xdgOutputManager = static_cast<zxdg_output_manager_v1*> (
	    wl_registry_bind (registry, name, &zxdg_output_manager_v1_interface, std::min (version, 3u))
	);
	driver->onXdgOutputManagerBound ();
    }
}

static void handleGlobalRemoved (void* data, struct wl_registry* registry, uint32_t id) {
    // only outputs come and go at runtime; onOutputRemoved ignores every other global
    static_cast<WaylandOpenGLDriver*> (data)->onOutputRemoved (id);
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

    if (!eglBindAPI (EGL_OPENGL_API)) {
	this->finishEGL ();
	sLog.exception ("eglBindAPI failed!");
    }

    const EGLint CONTEXT_ATTRIBUTES[] = {
	EGL_CONTEXT_MAJOR_VERSION_KHR,
	3,
	EGL_CONTEXT_MINOR_VERSION_KHR,
	3,
	EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,
	EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
	EGL_NONE,
    };

    m_eglContext.context
	= eglCreateContext (m_eglContext.display, m_eglContext.config, EGL_NO_CONTEXT, CONTEXT_ATTRIBUTES);

    if (m_eglContext.context == EGL_NO_CONTEXT) {
	this->finishEGL ();
	sLog.error ("eglCreateContext error " + std::to_string (eglGetError ()));
	sLog.exception ("eglCreateContext failed!");
    }
}

void WaylandOpenGLDriver::finishEGL () const {
    eglMakeCurrent (EGL_NO_DISPLAY, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (m_eglContext.display) {
	eglTerminate (m_eglContext.display);
    }
    eglReleaseThread ();
}

void WaylandOpenGLDriver::onLayerClose (Output::WaylandOutputViewport* viewport) {
    sLog.error ("Compositor closed our LS, freeing data...");

    // Usually the output is going away (monitor powered off or unplugged) and its removal follows.
    // Keep the viewport and its wl_output until then, so the wl_output listener never points at freed
    // memory. If the output stays instead, it is set up again (see scheduleSetupRetry).
    this->retrySetup (viewport);
}

void WaylandOpenGLDriver::retrySetup (Output::WaylandOutputViewport* viewport) {
    viewport->destroyLS ();
    Output::WaylandOutputSelection::scheduleSetupRetry (*viewport);

    if (this->viewportInFocus == viewport) {
	this->viewportInFocus = nullptr;
    }

    this->getOutput ().reset ();
}

void WaylandOpenGLDriver::onOutputAdded (uint32_t waylandName) {
    const auto viewport = new Output::WaylandOutputViewport (this, waylandName, m_waylandContext.registry);

    this->m_screens.emplace_back (viewport);

    // the initial outputs are set up by initWaylandRegistry and setupOutputLayerSurfaces
    if (!this->m_registryReady) {
	return;
    }

    if (m_waylandContext.xdgOutputManager) {
	viewport->setupXdgOutput (m_waylandContext.xdgOutputManager);
    }

    // its name arrives with wl_output.done; setupPendingOutputs creates the layer surface after that,
    // outside of event dispatching, since setupLS needs roundtrips of its own
    viewport->pendingSetup = true;
}

void WaylandOpenGLDriver::onXdgOutputManagerBound () {
    // during startup initWaylandRegistry calls this once the initial outputs are known
    if (!this->m_registryReady) {
	return;
    }

    for (const auto& viewport : this->m_screens) {
	if (!viewport->xdgOutput && !viewport->removed) {
	    viewport->setupXdgOutput (m_waylandContext.xdgOutputManager);
	}
    }
}

void WaylandOpenGLDriver::onOutputRemoved (uint32_t waylandName) {
    const auto it = Output::WaylandOutputSelection::findByWaylandName (this->m_screens, waylandName);

    if (it == this->m_screens.end () || (*it)->removed) {
	return;
    }

    const auto viewport = *it;

    sLog.out ("Output ", viewport->name, " removed");

    // only marked here: this can run inside setupLS's roundtrips for this very viewport.
    // freeRemovedOutputs deletes it from the event loop.
    this->releaseViewport (viewport);
    viewport->removed = true;
    viewport->pendingSetup = false;

    this->getOutput ().reset ();
}

void WaylandOpenGLDriver::releaseViewport (Output::WaylandOutputViewport* viewport) {
    viewport->destroyLS ();

    if (viewport->xdgOutput) {
	zxdg_output_v1_destroy (viewport->xdgOutput);
	viewport->xdgOutput = nullptr;
    }

    if (viewport->output) {
	wl_output_release (viewport->output);
	viewport->output = nullptr;
    }

    if (this->viewportInFocus == viewport) {
	this->viewportInFocus = nullptr;
    }
}

int WaylandOpenGLDriver::dispatchWaylandEvents () {
    const auto display = m_waylandContext.display;

    // Frame callbacks normally pace the loop, but they stop when nothing is drawn: every requested output off
    // or unplugged, a setup retry pending, or a compositor that stops sending them (an output in DPMS off).
    // wl_display_dispatch would then block until an event arrives, and as libwayland retries its poll on
    // EINTR, a SIGTERM couldn't stop the process either. So wait a bounded time, like wl_display_dispatch
    // otherwise does, so the main loop gets to check whether it should stop and pending setups get retried.
    while (wl_display_prepare_read (display) != 0) {
	if (wl_display_dispatch_pending (display) == -1) {
	    return -1;
	}
    }

    wl_display_flush (display);

    pollfd fd = { .fd = wl_display_get_fd (display), .events = POLLIN, .revents = 0 };

    const int ready = poll (&fd, 1, IDLE_DISPATCH_TIMEOUT_MS);

    if (ready > 0 && (fd.revents & POLLIN)) {
	if (wl_display_read_events (display) == -1) {
	    return -1;
	}
    } else {
	wl_display_cancel_read (display);

	// the connection to the compositor is gone
	if (ready > 0 && (fd.revents & (POLLHUP | POLLERR | POLLNVAL))) {
	    return -1;
	}

	// otherwise it timed out, or a signal interrupted it
    }

    return wl_display_dispatch_pending (display);
}

void WaylandOpenGLDriver::freeRemovedOutputs () {
    for (const auto viewport : Output::WaylandOutputSelection::takeRemoved (this->m_screens)) {
	delete viewport;
    }
}

void WaylandOpenGLDriver::setupPendingOutputs () {
    // setupLS does roundtrips, which can add outputs (handled next time) or remove them; removal only
    // marks them and they are freed at the end, so these pointers stay valid for the whole loop
    for (const auto viewport : Output::WaylandOutputSelection::outputsReadyForSetup (this->m_screens)) {
	if (viewport->removed || viewport->layerSurface) {
	    continue;
	}

	viewport->pendingSetup = false;

	if (!Output::WaylandOutputSelection::isOutputRequested (viewport->name, m_context)) {
	    continue;
	}

	sLog.out ("Output ", viewport->name, " appeared, setting up its layer surface");

	try {
	    viewport->setupLS ();
	} catch (const std::exception&) {
	    // already logged; keep the outputs that work running, and try this one again like a closed
	    // layer surface
	    this->retrySetup (viewport);
	    continue;
	}

	// removed or closed again during setupLS's roundtrips
	if (viewport->removed || !viewport->eglSurface) {
	    continue;
	}

	// a span group member that wasn't there at startup, or moved, changes the area the group covers
	this->getApp ().updateSpanBounds ();

	// draw the first frame; it requests the frame callback that keeps this output rendering
	viewport->rendering = true;
	this->getApp ().update (viewport);
	viewport->rendering = false;
    }

    this->freeRemovedOutputs ();
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

    // If xdg-output-manager is available, use it to get logical output positions
    // outputs announced from here on (including during the roundtrip below) get their own xdg_output
    // and are set up from the event loop
    m_registryReady = true;

    if (m_waylandContext.xdgOutputManager) {
	this->onXdgOutputManagerBound ();
    } else if (!m_context.settings.general.spanGroups.empty ()) {
	sLog.error ("zxdg_output_manager_v1 is unavailable; screen-span positions will be incorrect.");
    }

    // also delivers the name of outputs announced during the first roundtrip, which
    // setupOutputLayerSurfaces needs to pick the requested ones
    wl_display_roundtrip (m_waylandContext.display);
}

void WaylandOpenGLDriver::setupOutputLayerSurfaces () {
    bool any = false;
    // setupLS does roundtrips, which can add outputs to m_screens (those wait for setupPendingOutputs)
    const auto screens = this->m_screens;

    for (const auto& o : screens) {
	if (o->removed || !Output::WaylandOutputSelection::isOutputRequested (o->name, m_context)) {
	    continue;
	}

	// announced during the xdg-output roundtrip: set up here, not again from the event loop
	o->pendingSetup = false;
	o->setupLS ();
	// setupLS stops early if the output was removed or closed during its roundtrips
	any = any || o->layerSurface != nullptr;
    }

    this->freeRemovedOutputs ();

    if (!any) {
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
	    sLog.out ("Failed to initialize GLEW, but continuing with EGL context: No GLX display");
	} else {
	    const char* error = reinterpret_cast<const char*> (glewGetErrorString (result));
	    sLog.error ("Failed to initialize GLEW: ", error ? error : "Unknown error");
	    sLog.exception ("Cannot continue...");
	}
    }
}

WaylandOpenGLDriver::~WaylandOpenGLDriver () {
    // free the outputs while EGL is still up, destroyLS needs it. Take them out of m_screens first:
    // destroyLS looks through m_screens for another surface to make current
    const auto screens = std::exchange (this->m_screens, {});

    for (const auto& screen : screens) {
	this->releaseViewport (screen);
	delete screen;
    }

    // drop the output's pointers to them too (m_screens is empty now)
    this->getOutput ().reset ();

    // stop EGL
    eglMakeCurrent (EGL_NO_DISPLAY, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    if (m_eglContext.context != EGL_NO_CONTEXT) {
	eglDestroyContext (m_eglContext.display, m_eglContext.context);
    }

    eglTerminate (m_eglContext.display);
    eglReleaseThread ();

    // disconnect from wayland display
    if (this->m_waylandContext.display) {
	wl_display_disconnect (this->m_waylandContext.display);
    }
}

void WaylandOpenGLDriver::dispatchEventQueue () {
    static bool initialized = false;

    if (!initialized) {
	initialized = true;

	for (const auto& viewport : this->getOutput ().getViewports () | std::views::values) {
	    this->getApp ().update (viewport);
	}
    }

    // TODO: FRAMETIME CONTROL SHOULD GO BACK TO THE CWALLPAPAERAPPLICATION ONCE ACTUAL PARTICLES ARE IMPLEMENTED
    // TODO: AS THOSE, MORE THAN LIKELY, WILL REQUIRE OF A DIFFERENT PROCESSING RATE

    static float startTime, endTime, minimumTime = 1.0f / this->m_context.settings.render.maximumFPS;
    // get the start time of the frame
    startTime = this->getRenderTime ();

    if (this->dispatchWaylandEvents () == -1) {
	// the display is unusable after an error, so don't set up outputs on it
	m_requestedExit = true;
	return;
    }

    // outputs that (re)appeared while running, e.g. a monitor powered back on
    this->setupPendingOutputs ();

    m_frameCounter++;

    endTime = this->getRenderTime ();

    // ensure the frame time is correct to not overrun FPS
    if ((endTime - startTime) < minimumTime) {
	usleep ((minimumTime - (endTime - startTime)) * CLOCKS_PER_SEC);
    }
}

Output::Output& WaylandOpenGLDriver::getOutput () { return this->m_output; }

float WaylandOpenGLDriver::getRenderTime () const {
    return static_cast<float> (std::chrono::duration_cast<std::chrono::microseconds> (
				   std::chrono::high_resolution_clock::now () - renderStart
	   )
				   .count ())
	/ 1000000.0;
}

bool WaylandOpenGLDriver::closeRequested () { return this->m_requestedExit; }

void WaylandOpenGLDriver::resizeWindow (glm::ivec2 size) { }

void WaylandOpenGLDriver::resizeWindow (glm::ivec4 sizeandpos) { }

void WaylandOpenGLDriver::showWindow () { }

void WaylandOpenGLDriver::hideWindow () { }

glm::ivec2 WaylandOpenGLDriver::getFramebufferSize () const { return glm::ivec2 { 0, 0 }; }

uint32_t WaylandOpenGLDriver::getFrameCounter () const { return m_frameCounter; }

WaylandOpenGLDriver::SEGLContext* WaylandOpenGLDriver::getEGLContext () { return &this->m_eglContext; }

void* WaylandOpenGLDriver::getProcAddress (const char* name) const {
    return reinterpret_cast<void*> (eglGetProcAddress (name));
}

WaylandOpenGLDriver::WaylandContext* WaylandOpenGLDriver::getWaylandContext () { return &this->m_waylandContext; }

Output::WaylandOutputViewport* WaylandOpenGLDriver::surfaceToViewport (const wl_surface* surface) const {
    // events for a surface we already destroyed arrive with a null surface
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
