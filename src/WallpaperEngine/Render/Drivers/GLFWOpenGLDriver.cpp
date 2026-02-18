#include "GLFWOpenGLDriver.h"
#include "VideoFactories.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Render/Drivers/Output/GLFWWindowOutput.h"
#ifdef ENABLE_X11
#include "WallpaperEngine/Render/Drivers/Output/X11Output.h"
#endif

#define GLFW_EXPOSE_NATIVE_X11
#include "WallpaperEngine/Debugging/CallStack.h"

#include <GLFW/glfw3native.h>

#include <unistd.h>

using namespace WallpaperEngine::Render::Drivers;

void CustomGLFWErrorHandler (int errorCode, const char* reason) { sLog.error ("GLFW error ", errorCode, ": ", reason); }

GLFWOpenGLDriver::GLFWOpenGLDriver (const char* windowTitle, ApplicationContext& context, WallpaperApplication& app) :
    VideoDriver (app, m_mouseInput), m_context (context), m_mouseInput (*this) {
    glfwSetErrorCallback (CustomGLFWErrorHandler);

    // initialize glfw
    if (glfwInit () == GLFW_FALSE) {
	sLog.exception ("Failed to initialize glfw");
    }

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
    if (context.settings.render.mode == Application::ApplicationContext::EXPLICIT_WINDOW) {
	glfwWindowHint (GLFW_RESIZABLE, GLFW_FALSE);
	glfwWindowHint (GLFW_DECORATED, GLFW_FALSE);
	glfwWindowHint (GLFW_FLOATING, GLFW_TRUE);
    }

#if !NDEBUG
    glfwWindowHint (GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif /* DEBUG */

    // create window, size doesn't matter as long as we don't show it
    this->m_window = glfwCreateWindow (640, 480, windowTitle, nullptr, nullptr);

    if (this->m_window == nullptr) {
	sLog.exception ("Cannot create window");
    }

    // make context current, required for glew initialization
    glfwMakeContextCurrent (this->m_window);

    // initialize glew for rendering
    if (const GLenum result = glewInit (); result != GLEW_OK) {
	sLog.error ("Failed to initialize GLEW: ", glewGetErrorString (result));
    }

    // setup output
    if (context.settings.render.mode == ApplicationContext::EXPLICIT_WINDOW
	|| context.settings.render.mode == ApplicationContext::NORMAL_WINDOW) {
	m_output = new WallpaperEngine::Render::Drivers::Output::GLFWWindowOutput (context, *this);
    }
#ifdef ENABLE_X11
    else {
	m_output = new WallpaperEngine::Render::Drivers::Output::X11Output (context, *this);
    }
#else
    else {
	sLog.exception ("Trying to start GLFW in background mode without X11 support installed. Bailing out");
    }
#endif
}

GLFWOpenGLDriver::~GLFWOpenGLDriver () { glfwTerminate (); }

Output::Output& GLFWOpenGLDriver::getOutput () { return *this->m_output; }

float GLFWOpenGLDriver::getRenderTime () const { return static_cast<float> (glfwGetTime ()); }

bool GLFWOpenGLDriver::closeRequested () { return glfwWindowShouldClose (this->m_window); }

void GLFWOpenGLDriver::resizeWindow (glm::ivec2 size) { glfwSetWindowSize (this->m_window, size.x, size.y); }

void GLFWOpenGLDriver::resizeWindow (glm::ivec4 sizeandpos) {
    glfwSetWindowPos (this->m_window, sizeandpos.x, sizeandpos.y);
    glfwSetWindowSize (this->m_window, sizeandpos.z, sizeandpos.w);
}

void GLFWOpenGLDriver::showWindow () { glfwShowWindow (this->m_window); }

void GLFWOpenGLDriver::hideWindow () { glfwHideWindow (this->m_window); }

glm::ivec2 GLFWOpenGLDriver::getFramebufferSize () const {
    glm::ivec2 size;

    glfwGetFramebufferSize (this->m_window, &size.x, &size.y);

    return size;
}

uint32_t GLFWOpenGLDriver::getFrameCounter () const { return this->m_frameCounter; }

void GLFWOpenGLDriver::dispatchEventQueue () {
    static float startTime, endTime, minimumTime = 1.0f / this->m_context.settings.render.maximumFPS;
    // get the start time of the frame
    startTime = this->getRenderTime ();
    // clear the screen
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (const auto& [screen, viewport] : this->m_output->getViewports ()) {
	this->getApp ().update (viewport);
    }

    // read the full texture into the image
    if (this->m_output->haveImageBuffer ()) {
	// 4.5 supports glReadnPixels, anything older doesn't...
	if (GLEW_VERSION_4_5) {
	    glReadnPixels (
		0, 0, this->m_output->getFullWidth (), this->m_output->getFullHeight (), GL_BGRA, GL_UNSIGNED_BYTE,
		this->m_output->getImageBufferSize (), this->m_output->getImageBuffer ()
	    );
	} else {
	    // fallback to old version
	    glReadPixels (
		0, 0, this->m_output->getFullWidth (), this->m_output->getFullHeight (), GL_BGRA, GL_UNSIGNED_BYTE,
		this->m_output->getImageBuffer ()
	    );
	}

	GLenum error = glGetError ();

	if (error != GL_NO_ERROR) {
	    sLog.exception ("OpenGL error when reading texture ", error);
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
    if ((endTime - startTime) < minimumTime) {
	usleep ((minimumTime - (endTime - startTime)) * CLOCKS_PER_SEC);
    }
}

void* GLFWOpenGLDriver::getProcAddress (const char* name) const {
    return reinterpret_cast<void*> (glfwGetProcAddress (name));
}

GLFWwindow* GLFWOpenGLDriver::getWindow () const { return this->m_window; }

__attribute__ ((constructor)) void registerGLFWOpenGLDriver () {
    sVideoFactories.registerDriver (
	ApplicationContext::DESKTOP_BACKGROUND, "x11",
	[] (ApplicationContext& context, WallpaperApplication& application) -> std::unique_ptr<VideoDriver> {
	    return std::make_unique<GLFWOpenGLDriver> ("wallpaperengine", context, application);
	}
    );
    sVideoFactories.registerDriver (
	ApplicationContext::EXPLICIT_WINDOW, DEFAULT_WINDOW_NAME,
	[] (ApplicationContext& context, WallpaperApplication& application) -> std::unique_ptr<VideoDriver> {
	    return std::make_unique<GLFWOpenGLDriver> ("wallpaperengine", context, application);
	}
    );
    sVideoFactories.registerDriver (
	ApplicationContext::NORMAL_WINDOW, DEFAULT_WINDOW_NAME,
	[] (ApplicationContext& context, WallpaperApplication& application) -> std::unique_ptr<VideoDriver> {
	    return std::make_unique<GLFWOpenGLDriver> ("wallpaperengine", context, application);
	}
    );
}
