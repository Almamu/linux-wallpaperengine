#pragma once

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <wayland-egl.h>

#include <chrono>
#include <map>
#include <memory>

#include "WallpaperEngine/Desktop/Wayland/Output.h"
#include "WallpaperEngine/Desktop/Environment.h"

namespace WallpaperEngine::Application {
class ApplicationContext;
}
struct zwlr_layer_shell_v1;

namespace WallpaperEngine::Desktop::Wayland {
class Environment : public Desktop::Environment {
	friend class Output;
public:
	struct {
		EGLDisplay display;
		EGLConfig config;
		EGLContext context;
		PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC eglCreatePlatformWindowSurfaceEXT;
	} egl_context;
	struct {
		wl_display* display;
		wl_registry* registry;
		wl_compositor* compositor;
		wl_shm* shm;
		zwlr_layer_shell_v1* layerShell;
		wl_seat* seat;
	} wayland_context;

	Environment (Application::ApplicationContext& context);
	~Environment () override;

	void registerOutput (wl_registry* registry, uint32_t name);

	void render () override;
	void detectFullscreen () override;
	uint64_t getCurrentFrame () override;
	bool isCloseRequested () override;

	[[nodiscard]] Output* requestOutput (const std::string& name) override;
	[[nodiscard]] Output* getOutput (const std::string& name) override;

	std::chrono::high_resolution_clock::time_point render_start;

protected:
	void refreshOutputMap ();

private:
	void initEGL ();
	void finishEGL () const;

	uint64_t m_frameCount;
	bool m_requestedExit;

	Application::ApplicationContext& m_context;
	std::map<std::string, Output*> m_outputsByName;
	std::vector<Output*> m_outputs;
};
}
