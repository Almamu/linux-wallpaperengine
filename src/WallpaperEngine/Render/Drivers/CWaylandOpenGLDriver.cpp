#include "CWaylandOpenGLDriver.h"
#include "WallpaperEngine/Application/CWallpaperApplication.h"

#include <FreeImage.h>

#include "common.h"

#define class _class
#define namespace _namespace
#define static
extern "C" {
#include "xdg-shell-protocol.h"
#include "wlr-layer-shell-unstable-v1-protocol.h"
}
#undef class
#undef namespace
#undef static

#include <string.h>

using namespace WallpaperEngine::Render::Drivers;

static void handlePointerEnter (
    void* data, struct wl_pointer* wl_pointer, uint32_t serial, struct wl_surface* surface,
    wl_fixed_t surface_x, wl_fixed_t surface_y)
{
    const auto driver = static_cast <CWaylandOpenGLDriver*> (data);
    const auto viewport = driver->surfaceToViewport (surface);
    driver->viewportInFocus = viewport;
    wl_surface_set_buffer_scale(viewport->cursorSurface, viewport->scale);
    wl_surface_attach(viewport->cursorSurface, wl_cursor_image_get_buffer(viewport->pointer->images[0]), 0, 0);
    wl_pointer_set_cursor(wl_pointer, serial, viewport->cursorSurface, viewport->pointer->images[0]->hotspot_x, viewport->pointer->images[0]->hotspot_y);
    wl_surface_commit(viewport->cursorSurface);
}

static void handlePointerLeave(void* data, struct wl_pointer* wl_pointer, uint32_t serial, struct wl_surface* surface)
{
}

static void handlePointerAxis(void* data, wl_pointer* wl_pointer, uint32_t time, uint32_t axis, wl_fixed_t value)
{
}

static void handlePointerMotion(
    void* data, struct wl_pointer* wl_pointer, uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
    const auto driver = static_cast <CWaylandOpenGLDriver*> (data);

    auto x = wl_fixed_to_double(surface_x);
    auto y = wl_fixed_to_double(surface_y);

    if (!driver->viewportInFocus)
        return;

    driver->viewportInFocus->mousePos = {x * driver->viewportInFocus->scale, y * driver->viewportInFocus->scale};
}

static void handlePointerButton(
    void* data, struct wl_pointer* wl_pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t button_state)
{
}

const struct wl_pointer_listener pointerListener =
{
    .enter = handlePointerEnter,
    .leave = handlePointerLeave,
    .motion = handlePointerMotion,
    .button = handlePointerButton,
    .axis = handlePointerAxis
};

static void handleCapabilities(void* data, wl_seat* wl_seat, uint32_t capabilities)
{
    if (capabilities & WL_SEAT_CAPABILITY_POINTER)
        wl_pointer_add_listener(wl_seat_get_pointer(wl_seat), &pointerListener, data);
}

const struct wl_seat_listener seatListener =
{
    .capabilities = handleCapabilities
};

static void handleGlobal(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    const auto driver = static_cast <CWaylandOpenGLDriver*> (data);

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        driver->getWaylandContext()->compositor = (wl_compositor*)wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        driver->getWaylandContext()->shm = (wl_shm*)wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        driver->m_screens.emplace_back (new WallpaperEngine::Render::Drivers::Output::CWaylandOutputViewport (driver, name, registry));
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        driver->getWaylandContext()->layerShell = (zwlr_layer_shell_v1*)wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        driver->getWaylandContext()->seat = (wl_seat*)wl_registry_bind(registry, name, &wl_seat_interface, 1);
        wl_seat_add_listener(driver->getWaylandContext()->seat, &seatListener, driver);
    }
}

static void handleGlobalRemoved(void *data, struct wl_registry *registry, uint32_t id)
{
    // todo: outputs
}

const struct wl_registry_listener registryListener =
{
    .global = handleGlobal,
    .global_remove = handleGlobalRemoved,
};

void CWaylandOpenGLDriver::initEGL()
{
    const char* CLIENT_EXTENSIONS = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (!CLIENT_EXTENSIONS)
        sLog.exception("Failed to query EGL Extensions");

    const std::string CLIENTEXTENSIONS = std::string(CLIENT_EXTENSIONS);
    
    if (CLIENTEXTENSIONS.find("EGL_EXT_platform_base") == std::string::npos)
        sLog.exception("EGL_EXT_platform_base not supported by EGL!");

    if (CLIENTEXTENSIONS.find("EGL_EXT_platform_wayland") == std::string::npos)
        sLog.exception("EGL_EXT_platform_wayland not supported by EGL!");

    PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    m_eglContext.eglCreatePlatformWindowSurfaceEXT = (PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC)eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT");

    if (!eglGetPlatformDisplayEXT || !m_eglContext.eglCreatePlatformWindowSurfaceEXT)
        sLog.exception("EGL did not return EXT proc pointers!");

    auto deinitEGL = [&] () -> void {
        eglMakeCurrent(EGL_NO_DISPLAY, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (m_eglContext.display)
            eglTerminate(m_eglContext.display);
        eglReleaseThread();
    };

    m_eglContext.display = eglGetPlatformDisplayEXT(EGL_PLATFORM_WAYLAND_EXT, m_waylandContext.display, nullptr);

    if (m_eglContext.display == EGL_NO_DISPLAY) {
        deinitEGL();
        sLog.exception("eglGetPlatformDisplayEXT failed!");
    }

    if (!eglInitialize(m_eglContext.display, nullptr, nullptr)) {
        deinitEGL();
        sLog.exception("eglInitialize failed!");
    }

    const std::string CLIENTEXTENSIONSPOSTINIT = std::string(eglQueryString(m_eglContext.display, EGL_EXTENSIONS));

    if (CLIENTEXTENSIONSPOSTINIT.find("EGL_KHR_create_context") == std::string::npos) {
        deinitEGL();
        sLog.exception("EGL_KHR_create_context not supported!");
    }

    EGLint matchedConfigs = 0;
    const EGLint CONFIG_ATTRIBUTES[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_SAMPLES, 4,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE,
    };

    if (!eglChooseConfig(m_eglContext.display, CONFIG_ATTRIBUTES, &m_eglContext.config, 1, &matchedConfigs)) {
        deinitEGL();
        sLog.exception("eglChooseConfig failed!");
    }

    if (matchedConfigs == 0) {
        deinitEGL();
        sLog.exception("eglChooseConfig failed! (matched 0 configs)");
    }

    if (!eglBindAPI(EGL_OPENGL_API)) {
        deinitEGL();
        sLog.exception("eglBindAPI failed!");
    }

    const EGLint CONTEXT_ATTRIBUTES[] = {
        EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
        EGL_CONTEXT_MINOR_VERSION_KHR, 3,
        EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
        EGL_NONE,
    };
    m_eglContext.context = eglCreateContext(m_eglContext.display, m_eglContext.config, EGL_NO_CONTEXT, CONTEXT_ATTRIBUTES);

    if (m_eglContext.context == EGL_NO_CONTEXT) {
        sLog.error("eglCreateContext error " + std::to_string(eglGetError()));
        deinitEGL();
        sLog.exception("eglCreateContext failed!");
    }
}

void CWaylandOpenGLDriver::finishEGL()
{
    eglMakeCurrent(m_eglContext.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(m_eglContext.display, m_eglContext.context);
    eglTerminate(m_eglContext.display);
    eglReleaseThread();
}

void CWaylandOpenGLDriver::onLayerClose(Output::CWaylandOutputViewport* viewport)
{
    sLog.error ("Compositor closed our LS, freeing data...");

    if (viewport->eglSurface)
        eglDestroySurface (m_eglContext.display, viewport->eglSurface);

    if (viewport->eglWindow)
        wl_egl_window_destroy(viewport->eglWindow);

    if (viewport->layerSurface)
        zwlr_layer_surface_v1_destroy(viewport->layerSurface);

    if (viewport->surface)
        wl_surface_destroy(viewport->surface);

    // remove the output from the list
    std::remove (this->m_screens.begin (), this->m_screens.end (), viewport);
    // TODO: DELETE FROM VIEWPORT LIST

    delete viewport;
}

CWaylandOpenGLDriver::CWaylandOpenGLDriver(CApplicationContext& context, CWallpaperApplication& app) :
    m_frameCounter(0),
    m_fullscreenDetector (context, *this),
    m_output (context, *this),
    CVideoDriver (app)
{
    m_waylandContext.display = wl_display_connect (nullptr);

    if (!m_waylandContext.display)
        sLog.exception ("Failed to query wayland display");

    m_waylandContext.registry = wl_display_get_registry(m_waylandContext.display);
    wl_registry_add_listener(m_waylandContext.registry, &registryListener, this);

    wl_display_dispatch(m_waylandContext.display);
    wl_display_roundtrip(m_waylandContext.display);

    if (!m_waylandContext.compositor || !m_waylandContext.shm || !m_waylandContext.layerShell || this->m_screens.empty())
        sLog.exception ("Failed to bind to required interfaces");

    initEGL();

    bool any = false;

    for (auto& o : this->m_screens)
    {
        const auto cur = context.settings.general.screenBackgrounds.find (o->name);

        if (cur == context.settings.general.screenBackgrounds.end ())
            continue;

        o->setupLS ();
        any = true;
    }

    if (!any)
    {
        const auto cur = context.settings.general.screenBackgrounds.find ("auto");

        if (cur != context.settings.general.screenBackgrounds.end ())
        {
            // initializes the default screen only...
            m_screens [0]->setupLS ();
            any = true;
        }
    }

    if (!any)
        sLog.exception("No outputs could be initialized!");

    GLenum result = glewInit ();

    if (result != GLEW_OK)
        sLog.error("Failed to initialize GLEW: ", glewGetErrorString (result));

    FreeImage_Initialise (TRUE);
}

void CWaylandOpenGLDriver::dispatchEventQueue() const
{
    // render one frame to force drawing the screens
    this->getApp ().update ();

    wl_display_dispatch(m_waylandContext.display);
}

Detectors::CFullScreenDetector& CWaylandOpenGLDriver::getFullscreenDetector ()
{
    return this->m_fullscreenDetector;
}

Output::COutput& CWaylandOpenGLDriver::getOutput ()
{
    return this->m_output;
}

float CWaylandOpenGLDriver::getRenderTime () const
{
    return (float)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - renderStart).count() / 1000000.0;
}

bool CWaylandOpenGLDriver::closeRequested ()
{
    return false;
}

void CWaylandOpenGLDriver::resizeWindow (glm::ivec2 size)
{
}

void CWaylandOpenGLDriver::resizeWindow (glm::ivec4 sizeandpos)
{
}

void CWaylandOpenGLDriver::showWindow ()
{
}

void CWaylandOpenGLDriver::hideWindow ()
{
}

glm::ivec2 CWaylandOpenGLDriver::getFramebufferSize () const
{
    return glm::ivec2{0, 0};
}

void CWaylandOpenGLDriver::swapBuffers ()
{
    m_frameCounter ++;
}

uint32_t CWaylandOpenGLDriver::getFrameCounter () const
{
    return m_frameCounter;
}

CWaylandOpenGLDriver::SEGLContext* CWaylandOpenGLDriver::getEGLContext ()
{
    return &this->m_eglContext;
}

void* CWaylandOpenGLDriver::getProcAddress (const char* name) const
{
    return reinterpret_cast <void*> (eglGetProcAddress (name));
}

CWaylandOpenGLDriver::SWaylandContext* CWaylandOpenGLDriver::getWaylandContext ()
{
    return &this->m_waylandContext;
}

Output::CWaylandOutputViewport* CWaylandOpenGLDriver::surfaceToViewport(wl_surface* surface)
{
    for (auto& o : m_screens)
    {
        if (o->surface == surface)
            return o;
    }

    return nullptr;
}
