#include "Output.h"

#include "Environment.h"
#include "WallpaperEngine/Logging/Log.h"

#define class _class
#define namespace _namespace
#define static
#include "wlr-layer-shell-unstable-v1-protocol.h"
#undef static
#undef namespace
#undef class

using namespace WallpaperEngine::Desktop::Wayland;

static void handle_configure (void* data, zwlr_layer_surface_v1* layer_surface, uint32_t serial, uint32_t width, uint32_t height) {
	const auto output = static_cast<Output*> (data);

	output->size = { width, height };
	output->setViewport ({0, 0, width * output->scale, height * output->scale});
	zwlr_layer_surface_v1_ack_configure (layer_surface, serial);
}

static void handle_closed (void* data, zwlr_layer_surface_v1* surface) {
	// TODO: HANDLE ON LAYER CLOSE
}

static void geometry (void* data, wl_output* wl_output, int32_t x, int32_t y, int32_t physical_width, int32_t physical_height, int32_t subpixel, const char* make, const char* model, int32_t transform) {
	// ignored
}

static void mode (void* data, wl_output* wl_output, uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
	const auto output = static_cast<Output*> (data);

	output->size = { width, height };
	output->setViewport ({0, 0, width * output->scale, height * output->scale});

	// TODO: APPLY RESIZE
}

static void done (void* data, wl_output* wl_output) {
	// TODO: IMPLEMENT
}

static void scale (void* data, wl_output* wl_output, int32_t scale) {
	const auto output = static_cast<Output*> (data);

	output->scale = scale;
	output->setViewport ({0, 0, output->size.x * output->scale, output->size.y * output->scale});

	// TODO: APPLY RESIZE
}

static void name (void* data, wl_output* wl_output, const char* name) {
	const auto output = static_cast<Output*> (data);

	if (name) {
		output->name = name;
		output->notifyEnvironment ();
	}
}

static void description (void* data, wl_output* wl_output, const char* description) {
	// ignored
}

static void handle_frame_callback_done (void* data, wl_callback* cb, uint32_t time) {
	const auto output = static_cast<Output*> (data);

	output->frameCallback = nullptr;
	wl_callback_destroy (cb);
	output->render ();
}

constexpr wl_callback_listener frame_listener = {
	.done = handle_frame_callback_done,
};

constexpr wl_output_listener output_listener = {
	.geometry = geometry,
	.mode = mode,
	.done = done,
	.scale = scale,
	.name = name,
	.description = description,
};

constexpr zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = handle_configure,
	.closed = handle_closed
};

Output::Output(wl_registry* registry, uint32_t name, Environment& env) :
	Desktop::Output (nullptr, {0, 0, 1, 1}),
	name (""),
	scale (1),
	initialized (false),
	m_environment (env) {
	this->m_wl_output = static_cast<wl_output*> (wl_registry_bind (registry, name, &wl_output_interface, 4));
	wl_output_add_listener (this->m_wl_output, &output_listener, this);
}

void Output::render () {
	this->makeCurrent ();
	// render to the framebuffer first
	Desktop::Output::render ();
	// now swap it to the screen
	this->frameCallback = wl_surface_frame (this->m_wl_surface);
	wl_callback_add_listener (this->frameCallback, &frame_listener, this);
	eglSwapBuffers (this->m_environment.egl_context.display, this->m_egl_surface);
	wl_surface_set_buffer_scale (this->m_wl_surface, this->scale);
	wl_surface_damage_buffer (this->m_wl_surface, 0, 0, INT32_MAX, INT32_MAX);
	wl_surface_commit (this->m_wl_surface);
}

void Output::setupLayerShell () {
	this->m_wl_surface = wl_compositor_create_surface (this->m_environment.wayland_context.compositor);
	this->m_layerSurface = zwlr_layer_shell_v1_get_layer_surface (
		this->m_environment.wayland_context.layerShell, this->m_wl_surface, this->m_wl_output, ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM, "linux-wallpaperengine");

	if (this->m_layerSurface == nullptr) {
		sLog.exception ("Failed to create layer surface");
	}

	wl_region* region = wl_compositor_create_region (this->m_environment.wayland_context.compositor);

	// TODO: REPLACE THIS BY CHECKING IF MOUSE HAS TO BE ENABLED OR NOT
	if (false) {
		wl_region_add (region, 0, 0, INT32_MAX, INT32_MAX);
	}

	zwlr_layer_surface_v1_set_size (this->m_layerSurface, 0, 0);
	zwlr_layer_surface_v1_set_anchor (this->m_layerSurface, ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
	zwlr_layer_surface_v1_set_keyboard_interactivity (this->m_layerSurface, false);
	zwlr_layer_surface_v1_add_listener (this->m_layerSurface, &layer_surface_listener, this);
	zwlr_layer_surface_v1_set_exclusive_zone (this->m_layerSurface, -1);
	wl_surface_set_input_region (this->m_wl_surface, region);
	wl_region_destroy (region);
	wl_surface_commit (this->m_wl_surface);
	wl_display_roundtrip (this->m_environment.wayland_context.display);

	this->m_egl_window = wl_egl_window_create (this->m_wl_surface, this->size.x * this->scale, this->size.y * this->scale);
	this->m_egl_surface = this->m_environment.egl_context.eglCreatePlatformWindowSurfaceEXT (this->m_environment.egl_context.display, this->m_environment.egl_context.config, this->m_egl_window, nullptr);

	wl_surface_commit (this->m_wl_surface);
	wl_display_roundtrip (this->m_environment.wayland_context.display);
	wl_display_flush (this->m_environment.wayland_context.display);

	static const auto XCURSORSIZE = getenv ("XCURSOR_SIZE") ? std::stoi (getenv ("XCURSOR_SIZE")) : 24;
	const auto PRCURSORTHEME = wl_cursor_theme_load (getenv ("XCURSOR_THEME"), XCURSORSIZE * this->scale, this->m_environment.wayland_context.shm);

	if (PRCURSORTHEME == nullptr) {
		sLog.exception ("Failed to load cursor theme");
	}

	this->m_pointer = wl_cursor_theme_get_cursor (PRCURSORTHEME, "left_ptr");
	this->m_cursorSurface = wl_compositor_create_surface (this->m_environment.wayland_context.compositor);

	if (this->m_cursorSurface == nullptr) {
		sLog.exception ("Failed to create cursor surface");
	}

	if (eglMakeCurrent (this->m_environment.egl_context.display, this->m_egl_surface, this->m_egl_surface, this->m_environment.egl_context.context) == EGL_FALSE) {
		sLog.exception ("Failed to make EGL context current");
	}

	this->initialized = true;
}

void Output::notifyEnvironment () const {
	this->m_environment.refreshOutputMap ();
}

void Output::makeCurrent () const {
	if (eglMakeCurrent (this->m_environment.egl_context.display, this->m_egl_surface, this->m_egl_surface, this->m_environment.egl_context.context) == EGL_FALSE) {
		sLog.error ("eglMakeCurrent failed for output ", this->name);
	}
}