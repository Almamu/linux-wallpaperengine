#ifdef ENABLE_WAYLAND
#pragma once

#include <wayland-client.h>
#include <wayland-cursor.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glew.h>

#include <glm/vec2.hpp>
#include "COutputViewport.h"
#include "../CWaylandOpenGLDriver.h"

struct zwlr_layer_shell_v1;
struct zwlr_layer_surface_v1;

namespace WallpaperEngine::Render::Drivers
{
    class CWaylandOpenGLDriver;

    namespace Output
    {
        class COutputViewport;

        class CWaylandOutputViewport : public COutputViewport
        {
        public:
            CWaylandOutputViewport (CWaylandOpenGLDriver* driver, uint32_t waylandName, struct wl_registry *registry);

            /**
             * @return The wayland driver
             */
            CWaylandOpenGLDriver* getDriver ();

            wl_output* output;
            glm::ivec2 size;
            uint32_t waylandName;
            int scale = 1;
            bool initialized = false;
            bool rendering = false;

            wl_egl_window* eglWindow = nullptr;
            EGLSurface eglSurface = nullptr;
            wl_surface* surface = nullptr;
            zwlr_layer_surface_v1* layerSurface = nullptr;
            wl_callback* frameCallback = nullptr;
            glm::dvec2 mousePos = {0, 0};
            wl_cursor* pointer = nullptr;
            wl_surface* cursorSurface = nullptr;
            bool callbackInitialized = false;

            void setupLS ();

            /**
             * Activates output's context for drawing
             */
            void makeCurrent () override;

            /**
             * Swaps buffers to present data on the viewport
             */
            void swapOutput () override;

            /**
             * Updates the viewport size
             */
            void resize ();

        private:
            CWaylandOpenGLDriver* m_driver;
        };
    }
}
#endif /* ENABLE_WAYLAND */