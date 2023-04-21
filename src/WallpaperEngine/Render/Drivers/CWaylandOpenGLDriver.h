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
    class CLayerSurface;

    struct SWaylandOutput {
        wl_output* output;
        std::string name;
        glm::ivec2 size;
        glm::ivec2 lsSize;
        uint32_t waylandName;
        int scale = 1;
        CWaylandOpenGLDriver* driver = nullptr;
        bool initialized = false;
        std::unique_ptr<CLayerSurface> layerSurface;
        bool rendering = false;
    };

    class CLayerSurface {
    public:
        CLayerSurface(CWaylandOpenGLDriver*, SWaylandOutput*);
        ~CLayerSurface();

        wl_egl_window* eglWindow = nullptr;
        EGLSurface eglSurface = nullptr;
        wl_surface* surface = nullptr;
        zwlr_layer_surface_v1* layerSurface = nullptr;
        glm::ivec2 size;
        wl_callback* frameCallback = nullptr;
        SWaylandOutput* output = nullptr;
        glm::dvec2 mousePos = {0, 0};
        wl_cursor* pointer = nullptr;
        wl_surface* cursorSurface = nullptr;
        bool callbackInitialized = false;
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
        void makeCurrent(const std::string& outputName) const override;
        bool shouldRenderOutput(const std::string& outputName) const override;
        bool requiresSeparateFlips() const override;
        void swapOutputBuffer(const std::string& outputName) override;

        GLFWwindow* getWindow ();

        struct {
            wl_display* display = nullptr;
            wl_registry* registry = nullptr;
            wl_compositor* compositor = nullptr;
            wl_shm* shm = nullptr;
            zwlr_layer_shell_v1* layerShell = nullptr;
            wl_seat* seat = nullptr;
        } waylandContext;

        void onLayerClose(CLayerSurface*);
        void resizeLSSurfaceEGL(CLayerSurface*);
        CLayerSurface* surfaceToLS(wl_surface*);

        std::vector<std::unique_ptr<SWaylandOutput>> m_outputs;

        CWallpaperApplication* wallpaperApplication;

        struct {
            EGLDisplay display = nullptr;
            EGLConfig config = nullptr;
            EGLContext context = nullptr;
            PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC eglCreatePlatformWindowSurfaceEXT = nullptr;
        } eglContext;

        CLayerSurface* lastLSInFocus = nullptr;

    private:

        void initEGL();
        void finishEGL();

        uint32_t m_frameCounter;

        std::chrono::high_resolution_clock::time_point renderStart = std::chrono::high_resolution_clock::now();

        friend class CWaylandOutput;
        friend class CWallpaperApplication;
    };
}