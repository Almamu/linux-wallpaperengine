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

namespace WallpaperEngine::Application {
class ApplicationContext;
class WallpaperApplication;
} // namespace WallpaperEngine::Application

struct zwlr_layer_shell_v1;
struct zwlr_layer_surface_v1;

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
    };

    explicit WaylandOpenGLDriver (ApplicationContext& context, WallpaperApplication& app);
    ~WaylandOpenGLDriver ();

    [[nodiscard]] Output::Output& getOutput () override;
    float getRenderTime () const override;
    bool closeRequested () override;
    void resizeWindow (glm::ivec2 size) override;
    void resizeWindow (glm::ivec4 sizeandpos) override;
    void showWindow () override;
    void hideWindow () override;
    glm::ivec2 getFramebufferSize () const override;
    uint32_t getFrameCounter () const override;
    void dispatchEventQueue () override;
    [[nodiscard]] void* getProcAddress (const char* name) const override;

    void onLayerClose (Output::WaylandOutputViewport*);
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
    /** The Wayland context in use */
    WaylandContext m_waylandContext = {};
    mutable bool m_requestedExit;

    void initEGL ();
    void finishEGL () const;

    uint32_t m_frameCounter = 0;
    ApplicationContext& m_context;
    WaylandMouseInput m_mouseInput;

    std::chrono::high_resolution_clock::time_point renderStart = std::chrono::high_resolution_clock::now ();
};
} // namespace WallpaperEngine::Render::Drivers
#endif /* ENABLE_WAYLAND */