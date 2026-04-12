#pragma once

#include "WallpaperEngine/Desktop/Output.h"

#include <string>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <wayland-egl.h>
#include <glm/vec2.hpp>

struct zwlr_layer_surface_v1;

namespace WallpaperEngine::Desktop::Wayland {
class Environment;
class Output : public Desktop::Output {
public:
	Output(wl_registry* registry, uint32_t name, Environment& env);

	std::string name;
	int scale;
	bool initialized;
	glm::vec2 size;
	wl_callback* frameCallback;

	void render () override;

	void setupLayerShell ();
	void notifyEnvironment () const;
	void makeCurrent () const;

private:
	wl_output* m_wl_output;
	wl_egl_window* m_egl_window;
	EGLSurface m_egl_surface;
	wl_surface* m_wl_surface;
	zwlr_layer_surface_v1* m_layerSurface;
	wl_cursor* m_pointer;
	wl_surface* m_cursorSurface;
	Environment& m_environment;
};
}
