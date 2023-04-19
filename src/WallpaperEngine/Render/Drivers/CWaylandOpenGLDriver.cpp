#include "CWaylandOpenGLDriver.h"
#include "WallpaperEngine/Application/CWallpaperApplication.h"

#include <FreeImage.h>

#include "common.h"

#define class _class
#define namespace _namespace
#define static
extern "C" {
#include "xdg-shell-protocol.h"
#include "wlr-layer-shell-unstable-v1.h"
}
#undef class
#undef namespace
#undef static

#include <string.h>

using namespace WallpaperEngine::Render::Drivers;

void geometry(void* data, wl_output* output, int32_t x, int32_t y, int32_t width_mm, int32_t height_mm, int32_t subpixel, const char* make, const char* model,
                      int32_t transform) {
    // ignored
}

void mode(void* data, wl_output* output, uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
    const auto PMONITOR = (SWaylandOutput*)data;
    PMONITOR->size = {width, height};
}

void done(void* data, wl_output* wl_output) {
    const auto PMONITOR = (SWaylandOutput*)data;
}

void scale(void* data, wl_output* wl_output, int32_t scale) {
    const auto PMONITOR = (SWaylandOutput*)data;

    PMONITOR->scale = scale;
}

void name(void* data, wl_output* wl_output, const char* name) {
    const auto PMONITOR = (SWaylandOutput*)data;

    if (name)
        PMONITOR->name = name;
}

void description(void* data, wl_output* wl_output, const char* description) {
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
        wl_output_add_listener(POUTPUT->output, &outputListener, POUTPUT);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        PDRIVER->waylandContext.layerShell = (zwlr_layer_shell_v1*)wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
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
    const auto PDRIVER = (CWaylandOpenGLDriver*)data;
    PDRIVER->waylandContext.layerSurface.size = {w, h};

    if (PDRIVER->waylandContext.layerSurface.eglWindow)
        wl_egl_window_resize(PDRIVER->waylandContext.layerSurface.eglWindow, w, h, 0, 0);

    zwlr_layer_surface_v1_ack_configure(surface, serial);
}

static void handleLSClosed(void *data, zwlr_layer_surface_v1 *surface) {
    const auto PDRIVER = (CWaylandOpenGLDriver*)data;
    PDRIVER->onLayerClose();
}

void CWaylandOpenGLDriver::onLayerClose() {
    eglDestroySurface(eglContext.display, waylandContext.layerSurface.eglSurface);
    wl_egl_window_destroy(waylandContext.layerSurface.eglWindow);
    zwlr_layer_surface_v1_destroy(waylandContext.layerSurface.layerSurface);
    wl_surface_destroy(waylandContext.layerSurface.surface);
    sLog.exception("Compositor closed our LS!"); // todo: handle this?
}

const struct zwlr_layer_surface_v1_listener layerSurfaceListener = {
    .configure = handleLSConfigure,
    .closed = handleLSClosed,
};

static void surfaceFrameCallback(void *data, struct wl_callback *cb, uint32_t time) {
    const auto PDRIVER = (CWaylandOpenGLDriver*)data;
    wl_callback_destroy(cb);
    PDRIVER->waylandContext.layerSurface.frameCallback = nullptr;
    PDRIVER->wallpaperApplication->renderFrame();
}

const struct wl_callback_listener frameListener = {
    .done = surfaceFrameCallback
};

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

    const auto XCURSORSIZE = getenv("XCURSOR_SIZE") ? std::stoi(getenv("XCURSOR_SIZE")) : 24;
    const auto PRCURSORTHEME = wl_cursor_theme_load(NULL, XCURSORSIZE, waylandContext.shm);

    if (!PRCURSORTHEME)
        sLog.exception("Failed to get a cursor theme");

    waylandContext.pointer = wl_cursor_theme_get_cursor(PRCURSORTHEME, "left_ptr");
    waylandContext.cursorSurface = wl_compositor_create_surface(waylandContext.compositor);

    if (!waylandContext.cursorSurface)
        sLog.exception("Failed to get a cursor surface");

    initEGL();
    
    waylandContext.layerSurface.surface = wl_compositor_create_surface(waylandContext.compositor);
    waylandContext.layerSurface.layerSurface = zwlr_layer_shell_v1_get_layer_surface(waylandContext.layerShell, waylandContext.layerSurface.surface, m_outputs[0]->output, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "linux-wallpaperengine");

    if (!waylandContext.layerSurface.layerSurface) {
        finishEGL();
        sLog.exception("Failed to get a layer surface");
    }

    wl_region* region = wl_compositor_create_region(waylandContext.compositor);

    zwlr_layer_surface_v1_set_size(waylandContext.layerSurface.layerSurface, 0, 0);
    zwlr_layer_surface_v1_set_anchor(waylandContext.layerSurface.layerSurface, ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM);
    zwlr_layer_surface_v1_set_keyboard_interactivity(waylandContext.layerSurface.layerSurface, false);
    zwlr_layer_surface_v1_add_listener(waylandContext.layerSurface.layerSurface, &layerSurfaceListener, this);
    zwlr_layer_surface_v1_set_exclusive_zone(waylandContext.layerSurface.layerSurface, -1);
    wl_surface_set_input_region(waylandContext.layerSurface.surface, region);
    wl_surface_commit(waylandContext.layerSurface.surface);
    wl_display_roundtrip(waylandContext.display);

    waylandContext.layerSurface.eglWindow = wl_egl_window_create(waylandContext.layerSurface.surface, waylandContext.layerSurface.size.x, waylandContext.layerSurface.size.y);
    waylandContext.layerSurface.eglSurface = eglContext.eglCreatePlatformWindowSurfaceEXT(eglContext.display, eglContext.config, waylandContext.layerSurface.eglWindow, nullptr);
    wl_surface_commit(waylandContext.layerSurface.surface);
    wl_display_roundtrip(waylandContext.display);
    wl_display_flush(waylandContext.display);

    if (eglMakeCurrent(eglContext.display, waylandContext.layerSurface.eglSurface, waylandContext.layerSurface.eglSurface, eglContext.context) == EGL_FALSE) {
        finishEGL();
        sLog.exception("Failed to make egl current");
    }

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
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - renderStart).count() / 1000000;
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
    return waylandContext.layerSurface.size;
}

void CWaylandOpenGLDriver::swapBuffers () {
    waylandContext.layerSurface.frameCallback = wl_surface_frame(waylandContext.layerSurface.surface);
    wl_callback_add_listener(waylandContext.layerSurface.frameCallback, &frameListener, this);
    eglSwapBuffers(eglContext.display, waylandContext.layerSurface.eglSurface);
    wl_surface_set_buffer_scale(waylandContext.layerSurface.surface, 1);
    wl_surface_damage_buffer(waylandContext.layerSurface.surface, 0, 0, INT32_MAX, INT32_MAX);
    wl_surface_commit(waylandContext.layerSurface.surface);

    m_frameCounter++;
}

uint32_t CWaylandOpenGLDriver::getFrameCounter () const {
    return m_frameCounter;
}

GLFWwindow* CWaylandOpenGLDriver::getWindow () {
    return nullptr;
}

#include <iostream>

void CWaylandOpenGLDriver::makeCurrent() const {
    if (eglMakeCurrent(eglContext.display, waylandContext.layerSurface.eglSurface, waylandContext.layerSurface.eglSurface, eglContext.context) == EGL_FALSE) {
        std::cerr << "Couldn't make egl current";
    }
}