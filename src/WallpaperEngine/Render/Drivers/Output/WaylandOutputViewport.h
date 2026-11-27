#pragma once

#ifdef ENABLE_WAYLAND

#include <chrono>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glew.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <wayland-egl.h>

#include "../WaylandOpenGLDriver.h"
#include "OutputViewport.h"
#include <WallpaperEngine/Input/MouseInput.h>
#include <glm/vec2.hpp>

struct zwlr_layer_shell_v1;
struct zwlr_layer_surface_v1;
struct zxdg_output_v1;
struct zxdg_output_manager_v1;
struct wp_color_management_surface_v1;

namespace WallpaperEngine::Render::Drivers {
class WaylandOpenGLDriver;

namespace Output {
    class OutputViewport;

    class WaylandOutputViewport final : public OutputViewport {
    public:
	WaylandOutputViewport (WaylandOpenGLDriver* driver, uint32_t waylandName, struct wl_registry* registry, uint32_t version);

	/**
	 * @return The wayland driver
	 */
	WaylandOpenGLDriver* getDriver () const;

	wl_output* output = nullptr;
	glm::ivec2 size = {};
	glm::ivec2 position = {};
	uint32_t waylandName;
	int scale = 1;
	bool initialized = false;
	bool removed = false;
	int setupAttempts = 0;
	bool rendering = false;
	// New surfaces need a first frame before the compositor can send a callback.
	int frameCredits = 1;
	int refreshRate = 0; // Current output mode, in millihertz; zero means unknown.
	std::chrono::steady_clock::time_point nextFrameTime {};
	std::chrono::steady_clock::duration lastRenderDuration {};

	wl_egl_window* eglWindow = nullptr;
	EGLSurface eglSurface = nullptr;
	wl_surface* surface = nullptr;
	zwlr_layer_surface_v1* layerSurface = nullptr;
	wl_callback* frameCallback = nullptr;
	glm::dvec2 mousePos = { 0, 0 };
	bool hasMousePosition = false;
	WallpaperEngine::Input::MouseClickStatus leftClick = WallpaperEngine::Input::MouseClickStatus::Released;
	WallpaperEngine::Input::MouseClickStatus rightClick = WallpaperEngine::Input::MouseClickStatus::Released;
	wl_cursor* pointer = nullptr;
	wl_surface* cursorSurface = nullptr;
	bool callbackInitialized = false;
	bool hasXdgLogicalPosition = false;
	zxdg_output_v1* xdgOutput = nullptr;

	void setupLS (bool allowSDRFallback);
	void destroyLS ();
	void setupXdgOutput (zxdg_output_manager_v1* manager);

	/**
	 * Activates output's context for drawing
	 */
	void makeCurrent () override;

	/**
	 * Swaps buffers to present data on the viewport
	 */
	void swapOutput () override;
        bool supportsScRGB () const override;
        void setScRGBPresentation (bool enabled) override;

	int frameBudget () const;

	/**
	 * Updates the viewport size
	 */
	void resize ();

    private:
	WaylandOpenGLDriver* m_driver = nullptr;
	wl_cursor_theme* m_cursorTheme = nullptr;
        wp_color_management_surface_v1* m_colorSurface = nullptr;
        bool m_scRGBPresentation = false;
    };
} // namespace Output
} // namespace WallpaperEngine::Render::Drivers
#endif /* ENABLE_WAYLAND */
