#pragma once

#include <wayland-client.h>
#include <wayland-cursor.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glew.h>

#include "WallpaperEngine/Render/Drivers/CVideoDriver.h"
#include "WallpaperEngine/Application/CApplicationContext.h"

namespace WallpaperEngine::Application
{
    class CApplicationContext;
    class CWallpaperApplication;
}

struct GLFWwindow;
typedef GLFWwindow GLFWWindow;
struct zwlr_layer_shell_v1;
struct zwlr_layer_surface_v1;

namespace WallpaperEngine::Render::Drivers
{
    using namespace WallpaperEngine::Application;
    class CWaylandOpenGLDriver;

    struct SWaylandOutput {
        wl_output* output;
        std::string name;
        glm::ivec2 size;
        glm::ivec2 lsSize;
        uint32_t waylandName;
        int scale = 1;
        CWaylandOpenGLDriver* driver = nullptr;
        bool initialized = false;
    };

    class CWaylandOpenGLDriver : public CVideoDriver
    {
    public:
        explicit CWaylandOpenGLDriver (const char* windowTitle, CApplicationContext& context, CWallpaperApplication* app);
        ~CWaylandOpenGLDriver();

        void* getWindowHandle () const;
        float getRenderTime () const override;
        bool closeRequested () override;
        void resizeWindow (glm::ivec2 size) override;
        void resizeWindow (glm::ivec4 sizeandpos) override;
        void showWindow () override;
        void hideWindow () override;
        glm::ivec2 getFramebufferSize () const override;
        void swapBuffers () override;
        uint32_t getFrameCounter () const override;
        void dispatchEventQueue() const override;
        void makeCurrent() const;

        GLFWwindow* getWindow ();

        struct {
            wl_display* display = nullptr;
            wl_registry* registry = nullptr;
            wl_compositor* compositor = nullptr;
            wl_shm* shm = nullptr;
            zwlr_layer_shell_v1* layerShell = nullptr;
            wl_cursor* pointer = nullptr;
            wl_surface* cursorSurface = nullptr;

            struct {
                wl_egl_window* eglWindow = nullptr;
                EGLSurface eglSurface = nullptr;
                wl_surface* surface = nullptr;
                zwlr_layer_surface_v1* layerSurface = nullptr;
                glm::ivec2 size;
                wl_callback* frameCallback = nullptr;
                SWaylandOutput* output = nullptr;
            } layerSurface;
        } waylandContext;

        void onLayerClose();
        void resizeLSSurfaceEGL();

        std::vector<std::unique_ptr<SWaylandOutput>> m_outputs;

        CWallpaperApplication* wallpaperApplication;

    private:

        void initEGL();
        void finishEGL();

        struct {
            EGLDisplay display = nullptr;
            EGLConfig config = nullptr;
            EGLContext context = nullptr;
            PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC eglCreatePlatformWindowSurfaceEXT = nullptr;
        } eglContext;

        uint32_t m_frameCounter;

        std::chrono::high_resolution_clock::time_point renderStart = std::chrono::high_resolution_clock::now();

        friend class CWaylandOutput;
        friend class CWallpaperApplication;
    };
}