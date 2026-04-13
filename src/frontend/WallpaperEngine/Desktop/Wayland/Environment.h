#pragma once

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <wayland-egl.h>

#include <chrono>
#include <map>
#include <vector>
#include <string>
#include <memory>

#include "WallpaperEngine/Desktop/Environment.h"
#include "WallpaperEngine/Desktop/Wayland/Output.h"

namespace WallpaperEngine::Application {
class ApplicationContext;
}

struct zwlr_layer_shell_v1;
struct zwlr_foreign_toplevel_manager_v1;

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
		zwlr_foreign_toplevel_manager_v1* topLevelManager;
		wl_seat* seat;
		struct fullscreen_state {
			bool pending;
			bool current;
			bool pendingActivated;
			bool currentActivated;
			std::string appId;
			int fullscreenCount;
		} fullscreen_state;
	} wayland_context;

	Environment (
		Application::ApplicationContext& context, ScreenAvailableNotification& availableNotification,
		ScreenUnavailableNotification& unavailableNotification
	);
	~Environment () override;

	void registerOutput (wl_registry* registry, uint32_t name);
	void deregisterOutput (Output* output);

	void render () override;
	void detectFullscreen () override;
	uint64_t getCurrentFrame () override;
	bool isCloseRequested () override;

	std::chrono::high_resolution_clock::time_point render_start;

	bool isPendingRelevant () const;
	bool isCurrentRelevant () const;

protected:
	void refreshOutputMap ();

	bool isRelevant (const bool fullscreen, const bool activated, const std::string& appId) const;

private:
	void initEGL ();
	void finishEGL () const;

	uint64_t m_frameCount;
	bool m_requestedExit;

	std::map<std::string, Output*> m_outputs;
	/** List of outputs that are registered but don't have the necessary info yet */
	std::vector<Output*> m_stagingOutputs;
};
}
