#include "Environment.h"
#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Logging/Log.h"

#include <algorithm>
#include <cstring>

#define class _class
#define namespace _namespace
#define static
#include "wlr-foreign-toplevel-management-unstable-v1-protocol.h"
#include "wlr-layer-shell-unstable-v1-protocol.h"
#undef static
#undef namespace
#undef class

using namespace WallpaperEngine;
using namespace WallpaperEngine::Desktop::Wayland;

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

static void handle_toplevel_title (void* data, zwlr_foreign_toplevel_handle_v1* handle, const char* title) {
	// ignored
}

static void handle_toplevel_app_id (void* data, zwlr_foreign_toplevel_handle_v1* handle, const char* app_id) {
	const auto impl = static_cast<Environment*> (data);

	if (app_id) {
		impl->wayland_context.fullscreen_state.appId = app_id;
	}
}

static void handle_toplevel_output_enter (void* data, zwlr_foreign_toplevel_handle_v1* handle, wl_output* output) { }
static void handle_toplevel_output_leave (void* data, zwlr_foreign_toplevel_handle_v1* handle, wl_output* output) { }

static void handle_toplevel_state (void* data, zwlr_foreign_toplevel_handle_v1* handle, wl_array* state) {
	const auto impl = static_cast<Environment*> (data);
	const auto begin = static_cast<uint32_t*> (state->data);

	impl->wayland_context.fullscreen_state.pending = false;
	impl->wayland_context.fullscreen_state.pendingActivated = false;

	for (auto it = begin; it < begin + state->size / sizeof (uint32_t); ++it) {
		if (*it == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN) {
			impl->wayland_context.fullscreen_state.pending = true;
		}
		if (*it == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED) {
			impl->wayland_context.fullscreen_state.pendingActivated = true;
		}
	}
}

static void handle_toplevel_done (void* data, zwlr_foreign_toplevel_handle_v1* handle) {
	const auto impl = static_cast<Environment*> (data);

	// TODO: CHECK FOR RELEVANCY
	const bool pendingRelevant = impl->isPendingRelevant ();
	const bool currentRelevant = impl->isCurrentRelevant ();

	impl->wayland_context.fullscreen_state.current = impl->wayland_context.fullscreen_state.pending;
	impl->wayland_context.fullscreen_state.currentActivated = impl->wayland_context.fullscreen_state.pendingActivated;

	if (currentRelevant == pendingRelevant) {
		return;
	}

	if (pendingRelevant) {
		++impl->wayland_context.fullscreen_state.fullscreenCount;
		return;
	}

	if (impl->wayland_context.fullscreen_state.fullscreenCount == 0) {
		sLog.error ("Fullscreen count underflow!!");
	}

	--impl->wayland_context.fullscreen_state.fullscreenCount;
}

static void handle_toplevel_closed (void* data, zwlr_foreign_toplevel_handle_v1* handle) {
	const auto impl = static_cast<Environment*> (data);

	// TODO: CHECK FOR RELEVANCY
	if (impl->isCurrentRelevant ()) {
		if (impl->wayland_context.fullscreen_state.fullscreenCount == 0) {
			sLog.error ("Fullscreen count underflow!!");
		} else {
			impl->wayland_context.fullscreen_state.fullscreenCount--;
		}
	}

	zwlr_foreign_toplevel_handle_v1_destroy (handle);
}

static void
handle_toplevel_parent (void* data, zwlr_foreign_toplevel_handle_v1* handle, zwlr_foreign_toplevel_handle_v1* parent) {
	// ignored
}

constexpr zwlr_foreign_toplevel_handle_v1_listener toplevel_handle_listener = {
	.title = handle_toplevel_title,
	.app_id = handle_toplevel_app_id,
	.output_enter = handle_toplevel_output_enter,
	.output_leave = handle_toplevel_output_leave,
	.state = handle_toplevel_state,
	.done = handle_toplevel_done,
	.closed = handle_toplevel_closed,
	.parent = handle_toplevel_parent,
};

static void
handle_toplevel (void* data, zwlr_foreign_toplevel_manager_v1* manager, zwlr_foreign_toplevel_handle_v1* handle) {
	const auto impl = static_cast<Environment*> (data);

	impl->wayland_context.fullscreen_state = {
		.pending = false,
		.current = false,
		.pendingActivated = false,
		.currentActivated = false,
		.appId = "",
	};

	zwlr_foreign_toplevel_handle_v1_add_listener (handle, &toplevel_handle_listener, impl);
}

static void handle_finished (void* data, zwlr_foreign_toplevel_manager_v1* manager) {
	zwlr_foreign_toplevel_manager_v1_destroy (manager);
}

constexpr zwlr_foreign_toplevel_manager_v1_listener foreign_toplevel_manager_listener = {
	.toplevel = handle_toplevel,
	.finished = handle_finished,
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
	} else if (strcmp (interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0) {
		impl->wayland_context.topLevelManager = static_cast<zwlr_foreign_toplevel_manager_v1*> (
			wl_registry_bind (registry, name, &zwlr_foreign_toplevel_manager_v1_interface, 3)
		);
		zwlr_foreign_toplevel_manager_v1_add_listener (
			impl->wayland_context.topLevelManager, &foreign_toplevel_manager_listener, impl
		);
	}
}

static void handle_global_removed (void* data, wl_registry* registry, uint32_t name) {
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

void Environment::deregisterOutput (Output* output) {
	if (!output->name.empty ()) {
		const auto it = this->m_requestedOutputs.find (output->name);

		if (it != this->m_requestedOutputs.end ()) {
			it->second->setRealOutput (nullptr);
		}
	}

	std::erase (this->m_outputs, output);
}

void Environment::render () {
	// render happens on every surface, not really here, but a roundtrip could mean
	// a full render,so take it into account
	const float startTime = get_time (this);
	const float minimumTime = 1.0f / this->m_context.settings.render.maximumFPS;

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
	if (this->wayland_context.topLevelManager == nullptr) {
		return;
	}

	wl_display_roundtrip (this->wayland_context.display);

	this->anything_fullscreen = this->wayland_context.fullscreen_state.fullscreenCount > 0;
}

uint64_t Environment::getCurrentFrame () { return this->m_frameCount; }

bool Environment::isCloseRequested () { return this->m_requestedExit; }

Desktop::Output* Environment::requestOutput (const std::string& name) {
	// register the virtual output
	if (this->m_requestedOutputs.contains (name)) {
		sLog.exception ("Requested output ", name, " was already requested");
	}

	// check for a matching real output (if any)
	const auto realOutput = std::ranges::find_if (this->m_outputs, [&name] (const Output* output) {
		return output->name.compare (name) == 0;
	});

	auto newOutput = new VirtualOutput (realOutput == this->m_outputs.end () ? nullptr : *realOutput);

	if (realOutput != this->m_outputs.end ()) {
		if ((*realOutput)->initialized == false) {
			(*realOutput)->setupLayerShell ();
		}

		if ((*realOutput)->callbackInitialized == false) {
			(*realOutput)->render ();
		}
	}

	this->m_requestedOutputs.emplace (name, newOutput);

	return newOutput;
}

Desktop::Output* Environment::getOutput (const std::string& name) {
	const auto it = this->m_requestedOutputs.find (name);

	if (it == this->m_requestedOutputs.end ()) {
		sLog.exception ("Requested output ", name, " was not found");
	}

	return it->second;
}

bool Environment::isPendingRelevant () const {
	return this->isRelevant (
		this->wayland_context.fullscreen_state.pending, this->wayland_context.fullscreen_state.pendingActivated,
		this->wayland_context.fullscreen_state.appId
	);
}

bool Environment::isCurrentRelevant () const {
	return this->isRelevant (
		this->wayland_context.fullscreen_state.current, this->wayland_context.fullscreen_state.currentActivated,
		this->wayland_context.fullscreen_state.appId
	);
}

bool Environment::isRelevant (const bool fullscreen, const bool activated, const std::string& appId) const {
	if (!fullscreen) {
		return false;
	}

	if (this->m_context.settings.render.pauseOnFullscreenOnlyWhenActive && !activated) {
		return false;
	}

	if (appId.empty ()) {
		return true;
	}

	std::string lowercaseAppId;
	std::transform (appId.begin (), appId.end (), lowercaseAppId.begin (), ::tolower);

	for (const auto& ignore : this->m_context.settings.render.fullscreenPauseIgnoreAppIds) {
		if (ignore.empty ()) {
			continue;
		}

		std::string lowercaseIgnore;
		std::transform (ignore.begin (), ignore.end (), lowercaseIgnore.begin (), ::tolower);

		if (lowercaseAppId.find (lowercaseIgnore) != std::string::npos) {
			return false;
		}
	}

	return true;
}

void Environment::refreshOutputMap () {
	for (const auto& output : this->m_outputs) {
		if (output->name.empty ()) {
			continue;
		}

		const auto it = this->m_requestedOutputs.find (output->name);

		if (it == this->m_requestedOutputs.end ()) {
			continue;
		}

		it->second->setRealOutput (output);

		// ensure the output is initialized
		if (output->initialized == false) {
			output->setupLayerShell ();
		}

		if (output->callbackInitialized == true) {
			continue;
		}

		// starts the rendering to the output
		it->second->render ();
	}
}