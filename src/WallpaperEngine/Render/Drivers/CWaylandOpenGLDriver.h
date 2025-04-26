#pragma once

#ifdef ENABLE_WAYLAND

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glew.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <wayland-egl.h>

#include "WallpaperEngine/Application/CApplicationContext.h"
#include "WallpaperEngine/Application/CWallpaperApplication.h"
#include "WallpaperEngine/Input/Drivers/CWaylandMouseInput.h"
#include "WallpaperEngine/Render/Drivers/CVideoDriver.h"
#include "WallpaperEngine/Render/Drivers/Detectors/CWaylandFullScreenDetector.h"
#include "WallpaperEngine/Render/Drivers/Output/CWaylandOutput.h"
#include "WallpaperEngine/Render/Drivers/Output/CWaylandOutputViewport.h"

namespace WallpaperEngine::Application {
class CApplicationContext;
class CWallpaperApplication;
} // namespace WallpaperEngine::Application

namespace WallpaperEngine::Input::Drivers {
class CWaylandMouseInput;
}

struct zwlr_layer_shell_v1;
struct zwlr_layer_surface_v1;

namespace WallpaperEngine::Render::Drivers {
using namespace WallpaperEngine::Application;
using namespace WallpaperEngine::Input::Drivers;
class CWaylandOpenGLDriver;

namespace Output {
class CWaylandOutputViewport;
class CWaylandOutput;
} // namespace Output

class CWaylandOpenGLDriver final : public CVideoDriver {
    friend class Output::CWaylandOutput;
    friend class CWaylandMouseInput;

  public:
    struct SEGLContext {
        EGLDisplay display = nullptr;
        EGLConfig config = nullptr;
        EGLContext context = nullptr;
        PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC eglCreatePlatformWindowSurfaceEXT = nullptr;
    };

    struct SWaylandContext {
        wl_display* display = nullptr;
        wl_registry* registry = nullptr;
        wl_compositor* compositor = nullptr;
        wl_shm* shm = nullptr;
        zwlr_layer_shell_v1* layerShell = nullptr;
        wl_seat* seat = nullptr;
    };

    explicit CWaylandOpenGLDriver (CApplicationContext& context, CWallpaperApplication& app);
    ~CWaylandOpenGLDriver ();

    [[nodiscard]] Output::COutput& getOutput () override;
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

    void onLayerClose (Output::CWaylandOutputViewport*);
    Output::CWaylandOutputViewport* surfaceToViewport (const wl_surface*);

    Output::CWaylandOutputViewport* viewportInFocus = nullptr;

    [[nodiscard]] SEGLContext* getEGLContext ();
    [[nodiscard]] SWaylandContext* getWaylandContext ();

    /** List of available screens */
    std::vector<Output::CWaylandOutputViewport*> m_screens;

  private:
    /** The output used by the driver */
    Output::CWaylandOutput m_output;
    /** The EGL context in use */
    SEGLContext m_eglContext;
    /** The Wayland context in use */
    SWaylandContext m_waylandContext;
    mutable bool m_requestedExit;

    void initEGL ();
    void finishEGL () const;

    uint32_t m_frameCounter;
    CApplicationContext& m_context;
    CWaylandMouseInput m_mouseInput;

    std::chrono::high_resolution_clock::time_point renderStart = std::chrono::high_resolution_clock::now ();
};
} // namespace WallpaperEngine::Render::Drivers
#endif /* ENABLE_WAYLAND */