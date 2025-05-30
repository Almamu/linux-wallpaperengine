#include "CGLFWOpenGLDriver.h"
#include "CVideoFactories.h"
#include "WallpaperEngine/Logging/CLog.h"
#include "WallpaperEngine/Render/Drivers/Output/CGLFWWindowOutput.h"
#ifdef ENABLE_X11
#include "WallpaperEngine/Render/Drivers/Output/CX11Output.h"
#endif

#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

#include <unistd.h>

using namespace WallpaperEngine::Render::Drivers;

void CustomGLFWErrorHandler (int errorCode, const char* reason) {
    sLog.error ("GLFW error ", errorCode, ": ", reason);
}

CGLFWOpenGLDriver::CGLFWOpenGLDriver (
    const char* windowTitle, CApplicationContext& context, CWallpaperApplication& app
) :
    m_mouseInput (*this),
    CVideoDriver (app, m_mouseInput),
    m_context (context) {
    glfwSetErrorCallback (CustomGLFWErrorHandler);

    // initialize glfw
    if (glfwInit () == GLFW_FALSE)
        sLog.exception ("Failed to initialize glfw");

    // set some window hints (opengl version to be used)
    glfwWindowHint (GLFW_SAMPLES, 4);
    glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint (GLFW_VISIBLE, GLFW_FALSE);
    // set X11-specific hints
    glfwWindowHintString (GLFW_X11_CLASS_NAME, "linux-wallpaperengine");
    glfwWindowHintString (GLFW_X11_INSTANCE_NAME, "linux-wallpaperengine");

    // for forced window mode, we can set some hints that'll help position the window
    if (context.settings.render.mode == Application::CApplicationContext::EXPLICIT_WINDOW) {
        glfwWindowHint (GLFW_RESIZABLE, GLFW_FALSE);
        glfwWindowHint (GLFW_DECORATED, GLFW_FALSE);
        glfwWindowHint (GLFW_FLOATING, GLFW_TRUE);
    }

#if !NDEBUG
    glfwWindowHint (GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif /* DEBUG */

    // create window, size doesn't matter as long as we don't show it
    this->m_window = glfwCreateWindow (640, 480, windowTitle, nullptr, nullptr);

    if (this->m_window == nullptr)
        sLog.exception ("Cannot create window");

    // make context current, required for glew initialization
    glfwMakeContextCurrent (this->m_window);

    // initialize glew for rendering
    const GLenum result = glewInit ();

    if (result != GLEW_OK)
        sLog.error ("Failed to initialize GLEW: ", glewGetErrorString (result));

    // setup output
    if (context.settings.render.mode == CApplicationContext::EXPLICIT_WINDOW ||
        context.settings.render.mode == CApplicationContext::NORMAL_WINDOW) {
        m_output = new WallpaperEngine::Render::Drivers::Output::CGLFWWindowOutput (context, *this);
    }
#ifdef ENABLE_X11
    else {
        m_output = new WallpaperEngine::Render::Drivers::Output::CX11Output (context, *this);
    }
#else
    else {
        sLog.exception ("Trying to start GLFW in background mode without X11 support installed. Bailing out");
    }
#endif
}

CGLFWOpenGLDriver::~CGLFWOpenGLDriver () {
    glfwTerminate ();
}

Output::COutput& CGLFWOpenGLDriver::getOutput () {
    return *this->m_output;
}

float CGLFWOpenGLDriver::getRenderTime () const {
    return static_cast<float> (glfwGetTime ());
}

bool CGLFWOpenGLDriver::closeRequested () {
    return glfwWindowShouldClose (this->m_window);
}

void CGLFWOpenGLDriver::resizeWindow (glm::ivec2 size) {
    glfwSetWindowSize (this->m_window, size.x, size.y);
}

void CGLFWOpenGLDriver::resizeWindow (glm::ivec4 sizeandpos) {
    glfwSetWindowPos (this->m_window, sizeandpos.x, sizeandpos.y);
    glfwSetWindowSize (this->m_window, sizeandpos.z, sizeandpos.w);
}

void CGLFWOpenGLDriver::showWindow () {
    glfwShowWindow (this->m_window);
}

void CGLFWOpenGLDriver::hideWindow () {
    glfwHideWindow (this->m_window);
}

glm::ivec2 CGLFWOpenGLDriver::getFramebufferSize () const {
    glm::ivec2 size;

    glfwGetFramebufferSize (this->m_window, &size.x, &size.y);

    return size;
}

uint32_t CGLFWOpenGLDriver::getFrameCounter () const {
    return this->m_frameCounter;
}

void CGLFWOpenGLDriver::dispatchEventQueue () {
    static float startTime, endTime, minimumTime = 1.0f / this->m_context.settings.render.maximumFPS;
    // get the start time of the frame
    startTime = this->getRenderTime ();
    // clear the screen
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (const auto& [screen, viewport] : this->m_output->getViewports ())
        this->getApp ().update (viewport);

    // read the full texture into the image
    if (this->m_output->haveImageBuffer ()) {
        // 4.5 supports glReadnPixels, anything older doesn't...
        if (GLEW_VERSION_4_5) {
            glReadnPixels (0, 0, this->m_output->getFullWidth (), this->m_output->getFullHeight (), GL_BGRA,
                           GL_UNSIGNED_BYTE, this->m_output->getImageBufferSize (), this->m_output->getImageBuffer ());
        } else {
            // fallback to old version
            glReadPixels (0, 0, this->m_output->getFullWidth (), this->m_output->getFullHeight (), GL_BGRA, GL_UNSIGNED_BYTE, this->m_output->getImageBuffer ());
        }

        GLenum error = glGetError();

        if (error != GL_NO_ERROR) {
            sLog.exception("OpenGL error when reading texture ", error);
        }
    }

    // TODO: FRAMETIME CONTROL SHOULD GO BACK TO THE CWALLPAPAERAPPLICATION ONCE ACTUAL PARTICLES ARE IMPLEMENTED
    // TODO: AS THOSE, MORE THAN LIKELY, WILL REQUIRE OF A DIFFERENT PROCESSING RATE
    // update the output with the given image
    this->m_output->updateRender ();
    // do buffer swapping first
    glfwSwapBuffers (this->m_window);
    // poll for events
    glfwPollEvents ();
    // increase frame counter
    this->m_frameCounter++;
    // get the end time of the frame
    endTime = this->getRenderTime ();

    // ensure the frame time is correct to not overrun FPS
    if ((endTime - startTime) < minimumTime)
        usleep ((minimumTime - (endTime - startTime)) * CLOCKS_PER_SEC);
}

void* CGLFWOpenGLDriver::getProcAddress (const char* name) const {
    return reinterpret_cast<void*> (glfwGetProcAddress (name));
}

GLFWwindow* CGLFWOpenGLDriver::getWindow () const {
    return this->m_window;
}


__attribute__((constructor)) void registerGLFWOpenGLDriver () {
    sVideoFactories.registerDriver (
        CApplicationContext::DESKTOP_BACKGROUND,
        "x11",
        [](CApplicationContext& context, CWallpaperApplication& application) -> std::unique_ptr<CVideoDriver> {
            return std::make_unique <CGLFWOpenGLDriver> ("wallpaperengine", context, application);
        }
    );
    sVideoFactories.registerDriver (
        CApplicationContext::EXPLICIT_WINDOW,
        DEFAULT_WINDOW_NAME,
        [](CApplicationContext& context, CWallpaperApplication& application) -> std::unique_ptr<CVideoDriver> {
            return std::make_unique <CGLFWOpenGLDriver> ("wallpaperengine", context, application);
        }
    );
    sVideoFactories.registerDriver (
        CApplicationContext::NORMAL_WINDOW,
        DEFAULT_WINDOW_NAME,
        [](CApplicationContext& context, CWallpaperApplication& application) -> std::unique_ptr<CVideoDriver> {
            return std::make_unique <CGLFWOpenGLDriver> ("wallpaperengine", context, application);
        }
    );
}