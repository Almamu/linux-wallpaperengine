#ifdef ENABLE_WAYLAND
#pragma once

#include <wayland-client.h>
#include <wayland-cursor.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glew.h>

#include "WallpaperEngine/Render/Drivers/CVideoDriver.h"
#include "WallpaperEngine/Application/CApplicationContext.h"
#include "WallpaperEngine/Render/Drivers/Detectors/CWaylandFullScreenDetector.h"
#include "WallpaperEngine/Render/Drivers/Output/CWaylandOutputViewport.h"
#include "WallpaperEngine/Render/Drivers/Output/CWaylandOutput.h"

namespace WallpaperEngine::Application
{
    class CApplicationContext;
    class CWallpaperApplication;
}

namespace WallpaperEngine::Input::Drivers
{
    class CWaylandMouseInput;
}

struct zwlr_layer_shell_v1;
struct zwlr_layer_surface_v1;

namespace WallpaperEngine::Render::Drivers
{
    using namespace WallpaperEngine::Application;
    using namespace WallpaperEngine::Input::Drivers;
    class CWaylandOpenGLDriver;

    namespace Output
    {
        class CWaylandOutputViewport;
        class CWaylandOutput;
    }

    class CWaylandOpenGLDriver : public CVideoDriver
    {
        friend class Output::CWaylandOutput;
        friend class CWaylandMouseInput;
    public:
        struct SEGLContext
        {
            EGLDisplay display = nullptr;
            EGLConfig config = nullptr;
            EGLContext context = nullptr;
            PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC eglCreatePlatformWindowSurfaceEXT = nullptr;
        };

        struct SWaylandContext
        {
            wl_display* display = nullptr;
            wl_registry* registry = nullptr;
            wl_compositor* compositor = nullptr;
            wl_shm* shm = nullptr;
            zwlr_layer_shell_v1* layerShell = nullptr;
            wl_seat* seat = nullptr;
        };

        explicit CWaylandOpenGLDriver (CApplicationContext& context, CWallpaperApplication& app);
        ~CWaylandOpenGLDriver();

        [[nodiscard]] Detectors::CFullScreenDetector& getFullscreenDetector () override;
        [[nodiscard]] Output::COutput& getOutput () override;
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
        [[nodiscard]] void* getProcAddress (const char* name) const override;

        void onLayerClose(Output::CWaylandOutputViewport*);
        Output::CWaylandOutputViewport* surfaceToViewport(wl_surface*);

        Output::CWaylandOutputViewport* viewportInFocus = nullptr;

        [[nodiscard]] SEGLContext* getEGLContext ();
        [[nodiscard]] SWaylandContext* getWaylandContext ();


        /** List of available screens */
        std::vector <Output::CWaylandOutputViewport*> m_screens;

    private:
        /** Fullscreen detection used by this driver */
        Detectors::CWaylandFullScreenDetector m_fullscreenDetector;
        /** The output used by the driver */
        Output::CWaylandOutput m_output;
        /** The EGL context in use */
        SEGLContext m_eglContext;
        /** The Wayland context in use */
        SWaylandContext m_waylandContext;

        void initEGL();
        void finishEGL();

        uint32_t m_frameCounter;

        std::chrono::high_resolution_clock::time_point renderStart = std::chrono::high_resolution_clock::now();
    };
}
#endif /* ENABLE_WAYLAND */