#pragma once

#ifdef ENABLE_WAYLAND

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glew.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <wayland-egl.h>

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Application/WallpaperApplication.h"
#include "WallpaperEngine/Input/Drivers/WaylandMouseInput.h"
#include "WallpaperEngine/Render/Drivers/Detectors/WaylandFullScreenDetector.h"
#include "WallpaperEngine/Render/Drivers/Output/WaylandOutput.h"
#include "WallpaperEngine/Render/Drivers/Output/WaylandOutputViewport.h"
#include "WallpaperEngine/Render/Drivers/VideoDriver.h"
#include "WaylandColorManagement.h"
#include "WaylandEGLConfig.h"

namespace WallpaperEngine::Application {
class ApplicationContext;
class WallpaperApplication;
} // namespace WallpaperEngine::Application

struct zwlr_layer_shell_v1;
struct zwlr_layer_surface_v1;
struct zxdg_output_manager_v1;

namespace WallpaperEngine::Render::Drivers {
using namespace WallpaperEngine::Application;
using namespace WallpaperEngine::Input::Drivers;
class WaylandOpenGLDriver;

namespace Output {
    class WaylandOutputViewport;
    class WaylandOutput;
} // namespace Output

class WaylandOpenGLDriver final : public VideoDriver {
    friend class Output::WaylandOutput;
    friend class CWaylandMouseInput;

public:
    struct SEGLContext {
	EGLDisplay display = nullptr;
	EGLConfig config = nullptr;
	EGLConfig sdrConfig = nullptr;
	WaylandEGLConfigProperties properties;
	EGLContext context = nullptr;
	PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC eglCreatePlatformWindowSurfaceEXT = nullptr;
    };

    struct WaylandContext {
	wl_display* display = nullptr;
	wl_registry* registry = nullptr;
	wl_compositor* compositor = nullptr;
	wl_shm* shm = nullptr;
	zwlr_layer_shell_v1* layerShell = nullptr;
	wl_seat* seat = nullptr;
	zxdg_output_manager_v1* xdgOutputManager = nullptr;
	WaylandColorManagement colorManagement;
    };

    explicit WaylandOpenGLDriver (ApplicationContext& context, WallpaperApplication& app);
    ~WaylandOpenGLDriver ();

    [[nodiscard]] Output::Output& getOutput () override;
    double getRenderTime () const override;
    bool closeRequested () override;
    [[nodiscard]] bool abnormalTermination () const override;
    void resizeWindow (glm::ivec2 size) override;
    void resizeWindow (glm::ivec4 sizeandpos) override;
    void showWindow () override;
    void hideWindow () override;
    glm::ivec2 getFramebufferSize () const override;
    uint32_t getFrameCounter () const override;
    std::chrono::steady_clock::duration getLastFrameWait () const override;
    void dispatchEventQueue () override;
    void dispatchPendingEvents () override;
    [[nodiscard]] void* getProcAddress (const char* name) const override;
    bool makeBuildContextCurrent () override;
    void releaseBuildContext () override;

    // Only used during surface setup, before any wallpaper GL resources exist.
    bool fallbackToSDR ();
    void onLayerClose (Output::WaylandOutputViewport*);
    void onOutputAdded (uint32_t name, uint32_t version);
    void onOutputRemoved (uint32_t name);
    void onXdgOutputManagerBound ();
    void onOutputChanged () { m_outputChangePending = true; }
    Output::WaylandOutputViewport* surfaceToViewport (const wl_surface*) const;

    Output::WaylandOutputViewport* viewportInFocus = nullptr;

    [[nodiscard]] SEGLContext* getEGLContext ();
    [[nodiscard]] WaylandContext* getWaylandContext ();

    /** List of available screens */
    std::vector<Output::WaylandOutputViewport*> m_screens = {};

private:
    /** The output used by the driver */
    Output::WaylandOutput m_output;
    /** The EGL context in use */
    SEGLContext m_eglContext = {};
    /** Shared context for the async wallpaper build worker (lazily created on that thread) */
    EGLContext m_buildContext = EGL_NO_CONTEXT;
    /** 1x1 pbuffer fallback when EGL_KHR_surfaceless_context is unavailable */
    EGLSurface m_buildSurface = EGL_NO_SURFACE;
    /** The Wayland context in use */
    WaylandContext m_waylandContext = {};
    mutable bool m_requestedExit;
    /** The display connection failed and cannot recover within this process. */
    bool m_abnormalExit = false;
    bool m_outputChangePending = false;

    void initWaylandRegistry ();
    void setupOutputLayerSurfaces ();
    bool isOutputRequested (const std::string& name) const;
    void releaseLayerSurface (Output::WaylandOutputViewport* viewport);
    void releaseOutput (Output::WaylandOutputViewport* viewport);
    void removeDisconnectedOutputs ();
    void processOutputChanges ();
    void dispatchEvents (bool renderFrames);
    void initEGL ();
    EGLContext createEGLContext ();
    void initGLEW ();
    void finishEGL () const;

    uint32_t m_frameCounter = 0;
    std::chrono::steady_clock::time_point m_nextFrameToken {};
    std::chrono::steady_clock::duration m_lastFrameWait {};
    ApplicationContext& m_context;
    WaylandMouseInput m_mouseInput;

    std::chrono::high_resolution_clock::time_point renderStart = std::chrono::high_resolution_clock::now ();
};
} // namespace WallpaperEngine::Render::Drivers
#endif /* ENABLE_WAYLAND */
