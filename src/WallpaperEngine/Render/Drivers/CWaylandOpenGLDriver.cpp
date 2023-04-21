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

static void handlePointerEnter(void* data, struct wl_pointer* wl_pointer, uint32_t serial, struct wl_surface* surface, wl_fixed_t surface_x, wl_fixed_t surface_y) {
    const auto PDRIVER = (CWaylandOpenGLDriver*)data;
    const auto PLS = PDRIVER->surfaceToLS(surface);
    PDRIVER->lastLSInFocus = PLS;
    wl_surface_set_buffer_scale(PLS->cursorSurface, PLS->output->scale);
    wl_surface_attach(PLS->cursorSurface, wl_cursor_image_get_buffer(PLS->pointer->images[0]), 0, 0);
    wl_pointer_set_cursor(wl_pointer, serial, PLS->cursorSurface, PLS->pointer->images[0]->hotspot_x, PLS->pointer->images[0]->hotspot_y);
    wl_surface_commit(PLS->cursorSurface);
}

static void handlePointerLeave(void* data, struct wl_pointer* wl_pointer, uint32_t serial, struct wl_surface* surface) {
    // ignored
}

static void handlePointerAxis(void* data, wl_pointer* wl_pointer, uint32_t time, uint32_t axis, wl_fixed_t value) {
    // ignored
}

static void handlePointerMotion(void* data, struct wl_pointer* wl_pointer, uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
    auto x = wl_fixed_to_double(surface_x);
    auto y = wl_fixed_to_double(surface_y);

    const auto PDRIVER = (CWaylandOpenGLDriver*)data;
    if (!PDRIVER->lastLSInFocus)
        return;

    PDRIVER->lastLSInFocus->mousePos = {x * PDRIVER->lastLSInFocus->output->scale, y * PDRIVER->lastLSInFocus->output->scale};
}

static void handlePointerButton(void* data, struct wl_pointer* wl_pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t button_state) {
    // ignored
}

const struct wl_pointer_listener pointerListener = { .enter = handlePointerEnter, .leave = handlePointerLeave, .motion = handlePointerMotion, .button = handlePointerButton, .axis = handlePointerAxis };

static void handleCapabilities(void* data, wl_seat* wl_seat, uint32_t capabilities) {
    if (capabilities & WL_SEAT_CAPABILITY_POINTER)
        wl_pointer_add_listener(wl_seat_get_pointer(wl_seat), &pointerListener, data);
}

const struct wl_seat_listener seatListener = { .capabilities = handleCapabilities };

static void geometry(void* data, wl_output* output, int32_t x, int32_t y, int32_t width_mm, int32_t height_mm, int32_t subpixel, const char* make, const char* model,
                      int32_t transform) {
    // ignored
}

static void mode(void* data, wl_output* output, uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
    const auto PMONITOR = (SWaylandOutput*)data;
    PMONITOR->size = {width, height};
    PMONITOR->lsSize = {width, height};

    if (PMONITOR->layerSurface.get())
        PMONITOR->driver->resizeLSSurfaceEGL(PMONITOR->layerSurface.get());

    if (PMONITOR->initialized)
        PMONITOR->driver->wallpaperApplication->getOutput()->reset();
}

static void done(void* data, wl_output* wl_output) {
    const auto PMONITOR = (SWaylandOutput*)data;

    PMONITOR->initialized = true;
}

static void scale(void* data, wl_output* wl_output, int32_t scale) {
    const auto PMONITOR = (SWaylandOutput*)data;

    PMONITOR->scale = scale;

    if (PMONITOR->layerSurface.get())
        PMONITOR->driver->resizeLSSurfaceEGL(PMONITOR->layerSurface.get());

    if (PMONITOR->initialized)
        PMONITOR->driver->wallpaperApplication->getOutput()->reset();
}

static void name(void* data, wl_output* wl_output, const char* name) {
    const auto PMONITOR = (SWaylandOutput*)data;

    if (name)
        PMONITOR->name = name;
}

static void description(void* data, wl_output* wl_output, const char* description) {
    // ignored
}

const wl_output_listener outputListener = {.geometry = geometry, .mode = mode, .done = done, .scale = scale, .name = name, .description = description};

static void handleGlobal(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    const auto PDRIVER = (CWaylandOpenGLDriver*)data;

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        PDRIVER->waylandContext.compositor = (wl_compositor*)wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        PDRIVER->waylandContext.shm = (wl_shm*)wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        const auto POUTPUT = PDRIVER->m_outputs.emplace_back(std::make_unique<SWaylandOutput>()).get();
        POUTPUT->output = (wl_output*)wl_registry_bind(registry, name, &wl_output_interface, 4);
        POUTPUT->name = "";
        POUTPUT->size = {0, 0};
        POUTPUT->waylandName = name;
        POUTPUT->driver = PDRIVER;
        wl_output_add_listener(POUTPUT->output, &outputListener, POUTPUT);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        PDRIVER->waylandContext.layerShell = (zwlr_layer_shell_v1*)wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        PDRIVER->waylandContext.seat = (wl_seat*)wl_registry_bind(registry, name, &wl_seat_interface, 1);
        wl_seat_add_listener(PDRIVER->waylandContext.seat, &seatListener, PDRIVER);
    }
}

static void handleGlobalRemoved(void *data, struct wl_registry *registry, uint32_t id) {
    // todo: outputs
}

const struct wl_registry_listener registryListener = {
    .global = handleGlobal,
    .global_remove = handleGlobalRemoved,
};

void CWaylandOpenGLDriver::initEGL() {
    const char* CLIENT_EXTENSIONS = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (!CLIENT_EXTENSIONS)
        sLog.exception("Failed to query EGL Extensions");

    const std::string CLIENTEXTENSIONS = std::string(CLIENT_EXTENSIONS);
    
    if (CLIENTEXTENSIONS.find("EGL_EXT_platform_base") == std::string::npos)
        sLog.exception("EGL_EXT_platform_base not supported by EGL!");

    if (CLIENTEXTENSIONS.find("EGL_EXT_platform_wayland") == std::string::npos)
        sLog.exception("EGL_EXT_platform_wayland not supported by EGL!");

    PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    eglContext.eglCreatePlatformWindowSurfaceEXT = (PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC)eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT");

    if (!eglGetPlatformDisplayEXT || !eglContext.eglCreatePlatformWindowSurfaceEXT)
        sLog.exception("EGL did not return EXT proc pointers!");

    auto deinitEGL = [&] () -> void {
        eglMakeCurrent(EGL_NO_DISPLAY, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (eglContext.display)
            eglTerminate(eglContext.display);
        eglReleaseThread();
    };

    eglContext.display = eglGetPlatformDisplayEXT(EGL_PLATFORM_WAYLAND_EXT, waylandContext.display, nullptr);

    if (eglContext.display == EGL_NO_DISPLAY) {
        deinitEGL();
        sLog.exception("eglGetPlatformDisplayEXT failed!");
    }

    if (!eglInitialize(eglContext.display, nullptr, nullptr)) {
        deinitEGL();
        sLog.exception("eglInitialize failed!");
    }

    const std::string CLIENTEXTENSIONSPOSTINIT = std::string(eglQueryString(eglContext.display, EGL_EXTENSIONS));

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
    if (!eglChooseConfig(eglContext.display, CONFIG_ATTRIBUTES, &eglContext.config, 1, &matchedConfigs)) {
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
    eglContext.context = eglCreateContext(eglContext.display, eglContext.config, EGL_NO_CONTEXT, CONTEXT_ATTRIBUTES);

    if (eglContext.context == EGL_NO_CONTEXT) {
        sLog.error("eglCreateContext error " + std::to_string(eglGetError()));
        deinitEGL();
        sLog.exception("eglCreateContext failed!");
    }
}

void CWaylandOpenGLDriver::finishEGL() {
    eglMakeCurrent(eglContext.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(eglContext.display, eglContext.context);
    eglTerminate(eglContext.display);
    eglReleaseThread();
}


static void handleLSConfigure(void *data, zwlr_layer_surface_v1 *surface, uint32_t serial, uint32_t w, uint32_t h) {
    const auto PLS = (CLayerSurface*)data;
    PLS->size = {w, h};

    PLS->output->driver->resizeLSSurfaceEGL(PLS);

    zwlr_layer_surface_v1_ack_configure(surface, serial);
}

static void handleLSClosed(void *data, zwlr_layer_surface_v1 *surface) {
    const auto PLS = (CLayerSurface*)data;
    PLS->output->driver->onLayerClose(PLS);
}

void CWaylandOpenGLDriver::onLayerClose(CLayerSurface* layerSurface) {
    eglDestroySurface(eglContext.display, layerSurface->eglSurface);
    wl_egl_window_destroy(layerSurface->eglWindow);
    zwlr_layer_surface_v1_destroy(layerSurface->layerSurface);
    wl_surface_destroy(layerSurface->surface);
    sLog.exception("Compositor closed our LS!"); // todo: handle this?
}

const struct zwlr_layer_surface_v1_listener layerSurfaceListener = {
    .configure = handleLSConfigure,
    .closed = handleLSClosed,
};

static void surfaceFrameCallback(void *data, struct wl_callback *cb, uint32_t time) {
    const auto PLS = (CLayerSurface*)data;
    wl_callback_destroy(cb);
    PLS->frameCallback = nullptr;
    PLS->output->rendering = true;
    PLS->output->driver->wallpaperApplication->renderFrame();
    PLS->output->rendering = false;

    float renderTime = PLS->output->driver->getRenderTime();

    if ((renderTime - PLS->lastTime) < PLS->minimumTime)
        usleep ((PLS->minimumTime - (renderTime - PLS->lastTime)) * CLOCKS_PER_SEC);

    PLS->lastTime = renderTime;
}

const struct wl_callback_listener frameListener = {
    .done = surfaceFrameCallback
};

CLayerSurface::CLayerSurface(CWaylandOpenGLDriver* pDriver, SWaylandOutput* pOutput) {
    surface = wl_compositor_create_surface(pDriver->waylandContext.compositor);
    layerSurface = zwlr_layer_shell_v1_get_layer_surface(pDriver->waylandContext.layerShell, surface, pOutput->output, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "linux-wallpaperengine");
    output = pOutput;

    if (!layerSurface)
        sLog.exception("Failed to get a layer surface");

    wl_region* region = wl_compositor_create_region(pDriver->waylandContext.compositor);
    wl_region_add(region, 0, 0, INT32_MAX, INT32_MAX);

    zwlr_layer_surface_v1_set_size(layerSurface, 0, 0);
    zwlr_layer_surface_v1_set_anchor(layerSurface, ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM);
    zwlr_layer_surface_v1_set_keyboard_interactivity(layerSurface, false);
    zwlr_layer_surface_v1_add_listener(layerSurface, &layerSurfaceListener, this);
    zwlr_layer_surface_v1_set_exclusive_zone(layerSurface, -1);
    wl_surface_set_input_region(surface, region);
    wl_surface_commit(surface);
    wl_display_roundtrip(pDriver->waylandContext.display);

    eglWindow = wl_egl_window_create(surface, size.x * output->scale, size.y * output->scale);
    eglSurface = pDriver->eglContext.eglCreatePlatformWindowSurfaceEXT(pDriver->eglContext.display, pDriver->eglContext.config, eglWindow, nullptr);
    output->lsSize = size;
    wl_surface_commit(surface);
    wl_display_roundtrip(pDriver->waylandContext.display);
    wl_display_flush(pDriver->waylandContext.display);

    static const auto XCURSORSIZE = getenv("XCURSOR_SIZE") ? std::stoi(getenv("XCURSOR_SIZE")) : 24;
    const auto PRCURSORTHEME = wl_cursor_theme_load(getenv("XCURSOR_THEME"), XCURSORSIZE * output->scale, pDriver->waylandContext.shm);

    if (!PRCURSORTHEME)
        sLog.exception("Failed to get a cursor theme");

    pointer = wl_cursor_theme_get_cursor(PRCURSORTHEME, "left_ptr");
    cursorSurface = wl_compositor_create_surface(pDriver->waylandContext.compositor);

    if (!cursorSurface)
        sLog.exception("Failed to get a cursor surface");

    if (eglMakeCurrent(pDriver->eglContext.display, eglSurface, eglSurface, pDriver->eglContext.context) == EGL_FALSE)
        sLog.exception("Failed to make egl current");

    minimumTime = 1.0f / pDriver->wallpaperApplication->getContext().settings.render.maximumFPS;
}

CLayerSurface::~CLayerSurface() {
    ;
}

CWaylandOpenGLDriver::CWaylandOpenGLDriver(const char* windowTitle, CApplicationContext& context, CWallpaperApplication* app) : m_frameCounter(0) {
    wallpaperApplication = app;
    waylandContext.display = wl_display_connect(NULL);

    if (!waylandContext.display)
        sLog.exception("Failed to query wayland display");

    waylandContext.registry = wl_display_get_registry(waylandContext.display);
    wl_registry_add_listener(waylandContext.registry, &registryListener, this);

    wl_display_dispatch(waylandContext.display);
    wl_display_roundtrip(waylandContext.display);

    if (!waylandContext.compositor || !waylandContext.shm || !waylandContext.layerShell || m_outputs.empty())
        sLog.exception("Failed to bind to required interfaces");

    initEGL();

    bool any = false;
    for (auto& o : m_outputs) {
        if (std::find_if(context.settings.general.screenBackgrounds.begin(), context.settings.general.screenBackgrounds.end(), [&] (const auto& e) { return e.first == o->name; }) != context.settings.general.screenBackgrounds.end()) {
            o->layerSurface = std::make_unique<CLayerSurface>(this, o.get());
            any = true;
        }
    }

    if (!any && std::find_if(context.settings.general.screenBackgrounds.begin(), context.settings.general.screenBackgrounds.end(), [&] (const auto& e) { return e.first == "auto"; }) != context.settings.general.screenBackgrounds.end()) {
        m_outputs[0]->layerSurface = std::make_unique<CLayerSurface>(this, m_outputs[0].get());
        any = true;
    }

    if (!any)
        sLog.exception("No outputs could be initialized!");

    GLenum result = glewInit ();

    if (result != GLEW_OK)
        sLog.error("Failed to initialize GLEW: ", glewGetErrorString (result));

    FreeImage_Initialise (TRUE);
}

CWaylandOpenGLDriver::~CWaylandOpenGLDriver() {
    ;
}

void CWaylandOpenGLDriver::dispatchEventQueue() const {
    wl_display_dispatch(waylandContext.display);
}

void* CWaylandOpenGLDriver::getWindowHandle () const {
    return nullptr;
}

float CWaylandOpenGLDriver::getRenderTime () const {
    return (float)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - renderStart).count() / 1000000.0;
}

bool CWaylandOpenGLDriver::closeRequested () {
    return false;
}

void CWaylandOpenGLDriver::resizeWindow (glm::ivec2 size) {
    ;
}

void CWaylandOpenGLDriver::resizeWindow (glm::ivec4 sizeandpos) {
    ;
}

void CWaylandOpenGLDriver::showWindow () {
    ;
}

void CWaylandOpenGLDriver::hideWindow () {
    ;
}

glm::ivec2 CWaylandOpenGLDriver::getFramebufferSize () const {
    return glm::ivec2{0, 0};
}

void CWaylandOpenGLDriver::swapBuffers () {
    ;
}

void CWaylandOpenGLDriver::resizeLSSurfaceEGL(CLayerSurface* layerSurface) {
    if (layerSurface->eglWindow) {
        layerSurface->output->lsSize = layerSurface->size;

        wl_egl_window_resize(layerSurface->eglWindow, layerSurface->size.x * layerSurface->output->scale, layerSurface->size.y * layerSurface->output->scale, 0, 0);
        
        if (layerSurface->frameCallback) {
            wl_callback_destroy(layerSurface->frameCallback);
            layerSurface->frameCallback = nullptr;
        }

        wallpaperApplication->getOutput()->reset();
        
        layerSurface->output->rendering = true;
        wallpaperApplication->renderFrame();
        layerSurface->output->rendering = false;
    }
}

uint32_t CWaylandOpenGLDriver::getFrameCounter () const {
    return m_frameCounter;
}

GLFWwindow* CWaylandOpenGLDriver::getWindow () {
    return nullptr;
}

#include <iostream>

void CWaylandOpenGLDriver::makeCurrent(const std::string& outputName) const {
    for (auto& o : m_outputs) {
        if (o->name != outputName)
            continue;

        if (eglMakeCurrent(eglContext.display, o->layerSurface->eglSurface, o->layerSurface->eglSurface, eglContext.context) == EGL_FALSE) {
            std::cerr << "Couldn't make egl current";
        }
    }
}

CLayerSurface* CWaylandOpenGLDriver::surfaceToLS(wl_surface* surface) {
    for (auto& o : m_outputs) {
        if (!o->layerSurface.get())
            continue;

        if (o->layerSurface->surface == surface)
            return o->layerSurface.get();
    }

    return nullptr;
}

bool CWaylandOpenGLDriver::shouldRenderOutput(const std::string& outputName) const {
    for (auto& o : m_outputs) {
        if (o->name == outputName)
            return o->layerSurface.get() && (o->rendering || !o->layerSurface->callbackInitialized);
    }

    return false;
}

bool CWaylandOpenGLDriver::requiresSeparateFlips() const {
    return true;
}

void CWaylandOpenGLDriver::swapOutputBuffer(const std::string& outputName) {
    for (auto& o : m_outputs) {
        if (o->name != outputName)
            continue;

        o->layerSurface->callbackInitialized = true;

        eglMakeCurrent(eglContext.display, o->layerSurface->eglSurface, o->layerSurface->eglSurface, eglContext.context);
        o->layerSurface->frameCallback = wl_surface_frame(o->layerSurface->surface);
        wl_callback_add_listener(o->layerSurface->frameCallback, &frameListener, o->layerSurface.get());
        eglSwapBuffers(eglContext.display, o->layerSurface->eglSurface);
        wl_surface_set_buffer_scale(o->layerSurface->surface, o->scale);
        wl_surface_damage_buffer(o->layerSurface->surface, 0, 0, INT32_MAX, INT32_MAX);
        wl_surface_commit(o->layerSurface->surface);

        m_frameCounter++;
    }
}

std::string CWaylandOpenGLDriver::getCurrentlyRendered() const {
    for (auto& o : m_outputs) {
        if (o->rendering)
            return o->name;
    }

    return "";
}
