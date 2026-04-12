#include "Environment.h"
#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Logging/Log.h"

#include <cstring>

#define class _class
#define namespace _namespace
#define static
#include "wlr-foreign-toplevel-management-unstable-v1-protocol.h"
#include "wlr-layer-shell-unstable-v1-protocol.h"
#undef static
#undef namespace
#undef class

using namespace WallpaperEngine::Desktop::Wayland;

/*
#include "WaylandOpenGLDriver.h"
#include "VideoFactories.h"
#include "WallpaperEngine/Application/WallpaperApplication.h"
#include "WallpaperEngine/Logging/Log.h"

#define class _class
#define namespace _namespace
#define static
extern "C" {
#include "wlr-layer-shell-unstable-v1-protocol.h"
#include "xdg-shell-protocol.h"
#include <linux/input-event-codes.h>
}
#undef class
#undef namespace
#undef static

#include <string.h>
#include <unistd.h>

using namespace WallpaperEngine::Render::Drivers;

static void handlePointerEnter (
    void* data, struct wl_pointer* wl_pointer, uint32_t serial, struct wl_surface* surface, wl_fixed_t surface_x,
    wl_fixed_t surface_y
) {
    const auto driver = static_cast<WaylandOpenGLDriver*> (data);
    const auto viewport = driver->surfaceToViewport (surface);
    driver->viewportInFocus = viewport;
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
    driver->m_screens.emplace_back (
        new WallpaperEngine::Render::Drivers::Output::WaylandOutputViewport (driver, name, registry)
    );
    } else if (strcmp (interface, zwlr_layer_shell_v1_interface.name) == 0) {
    driver->getWaylandContext ()->layerShell
        = static_cast<zwlr_layer_shell_v1*> (wl_registry_bind (registry, name, &zwlr_layer_shell_v1_interface, 1));
    } else if (strcmp (interface, wl_seat_interface.name) == 0) {
    driver->getWaylandContext ()->seat
        = static_cast<wl_seat*> (wl_registry_bind (registry, name, &wl_seat_interface, 1));
    wl_seat_add_listener (driver->getWaylandContext ()->seat, &seatListener, driver);
    }
}

static void handleGlobalRemoved (void* data, struct wl_registry* registry, uint32_t id) {
    // todo: outputs
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

    if (viewport->eglSurface) {
    eglDestroySurface (m_eglContext.display, viewport->eglSurface);
    }

    if (viewport->eglWindow) {
    wl_egl_window_destroy (viewport->eglWindow);
    }

    if (viewport->layerSurface) {
    zwlr_layer_surface_v1_destroy (viewport->layerSurface);
    }

    if (viewport->surface) {
    wl_surface_destroy (viewport->surface);
    }

    // remove the output from the list
    std::erase (this->m_screens, viewport);

    // reset the viewports
    this->getOutput ().reset ();

    // finally free memory used by the viewport
    delete viewport;
}

WaylandOpenGLDriver::WaylandOpenGLDriver (ApplicationContext& context, WallpaperApplication& app) :
    VideoDriver (app, m_mouseInput), m_output (context, *this), m_requestedExit (false), m_frameCounter (0),
    m_context (context), m_mouseInput (*this) {
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

    initEGL ();

    bool any = false;

    for (const auto& o : this->m_screens) {
    if (!context.settings.general.screenBackgrounds.contains (o->name)) {
        continue;
    }

    o->setupLS ();
    any = true;
    }

    if (!any) {
    sLog.error ("No outputs could be initialized, please check the parameters and try again");
    sLog.error ("Detected outputs:");

    for (const auto& o : this->m_screens) {
        sLog.error ("  ", o->name);
    }

    sLog.error ("Requested: ");

    for (const auto& o : context.settings.general.screenBackgrounds | std::views::keys) {
        sLog.error ("  ", o);
    }

    sLog.exception ("Cannot continue...");
    }

    glewExperimental = GL_TRUE;
    if (const GLenum result = glewInit (); result != GLEW_OK) {
    if (result == GLEW_ERROR_NO_GLX_DISPLAY) {
        sLog.out ("Failed to initialize GLEW, but continuing with EGL context: No GLX display");
    } else {
        const char* error = reinterpret_cast<const char*>(glewGetErrorString (result));
        sLog.error ("Failed to initialize GLEW: ", error ? error : "Unknown error");
        sLog.exception ("Cannot continue...");
    }
    }
}

WaylandOpenGLDriver::~WaylandOpenGLDriver () {
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

    // TODO: WRITE A NON-BLOCKING VERSION OF THIS ONCE PARTICLE SIMULATION STARTS WORKING
    // TODO: OTHERWISE wl_display_dispatch WILL BLOCK IF NO SURFACES ARE BEING DRAWN
    static float startTime, endTime, minimumTime = 1.0f / this->m_context.settings.render.maximumFPS;
    // get the start time of the frame
    startTime = this->getRenderTime ();

    if (wl_display_dispatch (m_waylandContext.display) == -1) {
    m_requestedExit = true;
    }

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
}*/

static void handle_pointer_enter (
	void* data, wl_pointer* pointer, uint32_t serial, wl_surface* surface, wl_fixed_t surface_x, wl_fixed_t surface_y
) { }

static void handle_pointer_leave (void* data, wl_pointer* pointer, uint32_t serial, wl_surface* surface) { }

static void
handle_pointer_motion (void* data, wl_pointer* pointer, uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) { }

static void handle_pointer_button (
	void* data, wl_pointer* pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state
) { }

static void handle_pointer_axis (void* data, wl_pointer* pointer, uint32_t time, uint32_t axis, wl_fixed_t value) { }

constexpr wl_pointer_listener pointer_listener = {
	.enter = handle_pointer_enter,
	.leave = handle_pointer_leave,
	.motion = handle_pointer_motion,
	.button = handle_pointer_button,
	.axis = handle_pointer_axis,
};

static void handle_capabilities (void* data, wl_seat* seat, uint32_t capabilities) {
	if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
		wl_pointer_add_listener (wl_seat_get_pointer (seat), &pointer_listener, data);
	}
}

constexpr wl_seat_listener seat_listener = {
	.capabilities = handle_capabilities,
};

static void handle_global (void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
	const auto impl = static_cast<Environment*> (data);

	if (strcmp (interface, wl_compositor_interface.name) == 0) {
		impl->wayland_context.compositor
			= static_cast<wl_compositor*> (wl_registry_bind (registry, name, &wl_compositor_interface, 4));
	} else if (strcmp (interface, wl_shm_interface.name) == 0) {
		impl->wayland_context.shm = static_cast<wl_shm*> (wl_registry_bind (registry, name, &wl_shm_interface, 1));
	} else if (strcmp (interface, wl_output_interface.name) == 0) {
		impl->registerOutput (registry, name);
	} else if (strcmp (interface, zwlr_layer_shell_v1_interface.name) == 0) {
		impl->wayland_context.layerShell
			= static_cast<zwlr_layer_shell_v1*> (wl_registry_bind (registry, name, &zwlr_layer_shell_v1_interface, 1));
	} else if (strcmp (interface, wl_seat_interface.name) == 0) {
		impl->wayland_context.seat = static_cast<wl_seat*> (wl_registry_bind (registry, name, &wl_seat_interface, 1));
	}
}

static void handle_global_removed (void* data, wl_registry* registry, uint32_t id) {
	// TODO: outputs
}

constexpr wl_registry_listener registry_listener = {
	.global = handle_global,
	.global_remove = handle_global_removed,
};

static void* get_proc_address (void* user_parameter, const char* name) {
	return reinterpret_cast<void*> (eglGetProcAddress (name));
}

static float get_time (void* user_parameter) {
	const auto impl = static_cast<Environment*> (user_parameter);

	return static_cast<float> (std::chrono::duration_cast<std::chrono::microseconds> (
								   std::chrono::high_resolution_clock::now () - impl->render_start
		   )
	                               .count ())
		/ 1000000.0f;
}

Environment::Environment (WallpaperEngine::Application::ApplicationContext& context) : Desktop::Environment (context) {
	this->render_start = std::chrono::high_resolution_clock::now ();
	this->m_requestedExit = false;
	this->m_frameCount = 0;

	this->egl_context
		= { .display = nullptr, .config = nullptr, .context = nullptr, .eglCreatePlatformWindowSurfaceEXT = nullptr };
	this->wayland_context = { .display = nullptr,
		                      .registry = nullptr,
		                      .compositor = nullptr,
		                      .shm = nullptr,
		                      .layerShell = nullptr,
		                      .seat = nullptr };

	this->wayland_context.display = wl_display_connect (nullptr);

	if (this->wayland_context.display == nullptr) {
		sLog.exception ("Failed to query wayland display");
	}

	this->wayland_context.registry = wl_display_get_registry (this->wayland_context.display);
	wl_registry_add_listener (this->wayland_context.registry, &registry_listener, this);

	wl_display_dispatch (this->wayland_context.display);
	wl_display_roundtrip (this->wayland_context.display);

	if (this->wayland_context.compositor == nullptr || this->wayland_context.shm == nullptr
	    || this->wayland_context.layerShell == nullptr || this->wayland_context.seat == nullptr) {
		sLog.exception ("Failed to bind to required interfaces");
	}

	// TODO: REST OF EGL INITIALIZATION
	this->initEGL ();

	// initialize glad
	if (!gladLoadGLLoader (reinterpret_cast<GLADloadproc> (eglGetProcAddress))) {
		sLog.exception ("Failed to initialize glad");
	}

	this->counter = { .user_parameter = this, .get_time = get_time };

	this->gl_proc_address = {
		.user_parameter = this,
		.get_proc_address = get_proc_address,
	};

	this->mouse_input = { .user_parameter = this, .get_x = nullptr, .get_y = nullptr, .is_pressed = nullptr };
}

Environment::~Environment () {
	this->finishEGL ();

	if (this->wayland_context.display) {
		wl_display_disconnect (this->wayland_context.display);
	}
}

void Environment::initEGL () {
	const char* CLIENT_EXTENSIONS = eglQueryString (EGL_NO_DISPLAY, EGL_EXTENSIONS);

	if (!CLIENT_EXTENSIONS) {
		sLog.exception ("Failed to query EGL extensions");
	}

	const auto CLIENTEXTENSIONS = std::string (CLIENT_EXTENSIONS);

	if (CLIENTEXTENSIONS.find ("EGL_EXT_platform_base") == std::string::npos) {
		sLog.exception ("EGL_EXT_platform_base not supported!");
	}

	if (CLIENTEXTENSIONS.find ("EGL_EXT_platform_wayland") == std::string::npos) {
		sLog.exception ("EGL_EXT_platform_wayland not supported!");
	}

	const auto eglGetPlatformDisplayEXT
		= reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC> (eglGetProcAddress ("eglGetPlatformDisplayEXT"));
	this->egl_context.eglCreatePlatformWindowSurfaceEXT = reinterpret_cast<PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC> (
		eglGetProcAddress ("eglCreatePlatformWindowSurfaceEXT")
	);

	if (eglGetPlatformDisplayEXT == nullptr) {
		sLog.exception ("Failed to get eglGetPlatformDisplayEXT function pointer");
	}

	if (this->egl_context.eglCreatePlatformWindowSurfaceEXT == nullptr) {
		sLog.exception ("Failed to get eglCreatePlatformWindowSurfaceEXT function pointer");
	}

	this->egl_context.display
		= eglGetPlatformDisplayEXT (EGL_PLATFORM_WAYLAND_EXT, this->wayland_context.display, nullptr);

	if (this->egl_context.display == EGL_NO_DISPLAY) {
		this->finishEGL ();
		sLog.exception ("Failed to get EGL display");
	}

	if (!eglInitialize (this->egl_context.display, nullptr, nullptr)) {
		this->finishEGL ();
		sLog.exception ("Failed to initialize EGL");
	}

	const auto CLIENTEXTENSIONSPOSTINIT = std::string (eglQueryString (this->egl_context.display, EGL_EXTENSIONS));

	if (CLIENTEXTENSIONSPOSTINIT.find ("EGL_KHR_create_context") == std::string::npos) {
		this->finishEGL ();
		sLog.exception ("EGL_KHR_create_context not supported!");
	}

	EGLint matchedConfigs = 0;
	const EGLint configAttribs[]
		= { EGL_SURFACE_TYPE,    EGL_WINDOW_BIT, EGL_RED_SIZE, 1, EGL_GREEN_SIZE, 1, EGL_BLUE_SIZE, 1, EGL_SAMPLES, 4,
		    EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_NONE };

	if (!eglChooseConfig (this->egl_context.display, configAttribs, &this->egl_context.config, 1, &matchedConfigs)) {
		this->finishEGL ();
		sLog.exception ("Failed to choose EGL config");
	}

	if (matchedConfigs == 0) {
		this->finishEGL ();
		sLog.exception ("Failed to find a suitable EGL config");
	}

	if (!eglBindAPI (EGL_OPENGL_API)) {
		this->finishEGL ();
		sLog.exception ("Failed to bind OpenGL API");
	}

	const EGLint contextAttribs[] = { EGL_CONTEXT_MAJOR_VERSION_KHR,
		                              3,
		                              EGL_CONTEXT_MINOR_VERSION_KHR,
		                              3,
		                              EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,
		                              EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
		                              EGL_NONE };

	this->egl_context.context
		= eglCreateContext (this->egl_context.display, this->egl_context.config, EGL_NO_CONTEXT, contextAttribs);

	if (this->egl_context.context == EGL_NO_CONTEXT) {
		this->finishEGL ();
		sLog.error ("eglCreateContext error ", eglGetError ());
		sLog.exception ("Failed to create EGL context");
	}

	eglMakeCurrent (this->egl_context.display, EGL_NO_SURFACE, EGL_NO_SURFACE, this->egl_context.context);
}

void Environment::finishEGL () const {
	eglMakeCurrent (EGL_NO_DISPLAY, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

	if (this->egl_context.context != EGL_NO_CONTEXT) {
		eglDestroyContext (this->egl_context.display, this->egl_context.context);
	}

	if (this->egl_context.display) {
		eglTerminate (this->egl_context.display);
	}

	eglReleaseThread ();
}

void Environment::registerOutput (wl_registry* registry, uint32_t name) {
	this->m_outputs.push_back (new Output (registry, name, *this));
}

void Environment::render () {
	static bool initialized = false;
	// render happens on every surface, not really here, but a roundtrip could mean
	// a full render,so take it into account
	const float startTime = get_time (this);
	const float minimumTime = 1.0f / this->m_context.settings.render.maximumFPS;

	// this starts the event chain of frame_listener, so it has to happen only once
	// TODO: SUPPORT HOTPLUGGING OF SCREENS
	if (!initialized) {
		initialized = true;

		for (const auto& output : this->m_outputs) {
			if (output->initialized == false) {
				continue;
			}

			// perform a full render of the output, this will start the appropiate chain of events
			output->render ();
		}
	}

	if (wl_display_dispatch (this->wayland_context.display) == -1) {
		this->m_requestedExit = true;
	}

	this->m_frameCount++;
	const float endTime = get_time (this);
	const float delta = endTime - startTime;

	if (delta < minimumTime) {
		usleep ((minimumTime - delta) * CLOCKS_PER_SEC);
	}
}

void Environment::detectFullscreen () {
	// TODO: IMPLEMENT
}

uint64_t Environment::getCurrentFrame () {
	// TODO: IMPLEMENT
	return this->m_frameCount;
}

bool Environment::isCloseRequested () {
	// TODO: IMPLEMENT
	return this->m_requestedExit;
}

Output* Environment::requestOutput (const std::string& name) {
	const auto it = this->m_outputsByName.find (name);

	if (it == this->m_outputsByName.end ()) {
		sLog.exception ("Requested output ", name, " was not found");
	}

	if (it->second->initialized == false) {
		it->second->setupLayerShell ();
	}

	return it->second;
}

Output* Environment::getOutput (const std::string& name) {
	const auto it = this->m_outputsByName.find (name);

	if (it == this->m_outputsByName.end ()) {
		sLog.exception ("Requested output ", name, " was not found");
	}

	return it->second;
}

void Environment::refreshOutputMap () {
	this->m_outputsByName.clear ();

	for (const auto& output : this->m_outputs) {
		if (output->name.empty ()) {
			continue;
		}

		this->m_outputsByName.emplace (output->name, output);
	}
}