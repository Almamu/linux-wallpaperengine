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

#ifdef ENABLE_X11
#include <X11/Xlib.h>
#endif
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
    // The desktop path uses its own single-sample render target. Keeping the
    // hidden GLX drawable tiny and non-multisampled avoids wasting GPU memory.
    glfwWindowHint (GLFW_SAMPLES, context.settings.render.mode == ApplicationContext::DESKTOP_BACKGROUND ? 0 : 4);
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

    const glm::ivec2 initialSize = context.settings.render.mode == ApplicationContext::DESKTOP_BACKGROUND
	? glm::ivec2 { 64, 64 }
	: glm::ivec2 { 640, 480 };

    this->m_window = glfwCreateWindow (initialSize.x, initialSize.y, windowTitle, nullptr, nullptr);

    if (this->m_window == nullptr) {
	sLog.exception ("Cannot create window");
    }

#ifdef ENABLE_X11
    if (context.settings.render.mode == ApplicationContext::DESKTOP_BACKGROUND) {
	if (Display* display = glfwGetX11Display (); display != nullptr) {
	    XSetWindowAttributes attributes {};
	    attributes.override_redirect = True;
	    XChangeWindowAttributes (display, glfwGetX11Window (this->m_window), CWOverrideRedirect, &attributes);
	    XFlush (display);
	}
    }
#endif

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

GLFWOpenGLDriver::~GLFWOpenGLDriver () {
    glfwMakeContextCurrent (this->m_window);
    this->getApp ().setDestinationFramebuffer (0);

    if (this->m_x11ColorBuffer != 0)
	glDeleteRenderbuffers (1, &this->m_x11ColorBuffer);
    if (this->m_x11Framebuffer != 0)
	glDeleteFramebuffers (1, &this->m_x11Framebuffer);

    delete this->m_output;
    glfwTerminate ();
}

Output::Output& GLFWOpenGLDriver::getOutput () { return *this->m_output; }

float GLFWOpenGLDriver::getRenderTime () const { return static_cast<float> (glfwGetTime ()); }

bool GLFWOpenGLDriver::closeRequested () { return glfwWindowShouldClose (this->m_window); }

void GLFWOpenGLDriver::resizeWindow (glm::ivec2 size) { glfwSetWindowSize (this->m_window, size.x, size.y); }

void GLFWOpenGLDriver::resizeWindow (glm::ivec4 sizeandpos) {
    glfwSetWindowPos (this->m_window, sizeandpos.x, sizeandpos.y);
    glfwSetWindowSize (this->m_window, sizeandpos.z, sizeandpos.w);
}

void GLFWOpenGLDriver::ensureX11RenderTargetSize (glm::ivec2 size) {
    if (size.x <= 0 || size.y <= 0)
	sLog.exception ("Invalid X11 render target size: ", size.x, "x", size.y);

    if (this->m_x11Framebuffer != 0 && this->m_x11RenderTargetSize == size)
	return;

    GLint previousFramebuffer = 0;
    GLint previousRenderbuffer = 0;
    glGetIntegerv (GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
    glGetIntegerv (GL_RENDERBUFFER_BINDING, &previousRenderbuffer);

    if (this->m_x11ColorBuffer != 0)
	glDeleteRenderbuffers (1, &this->m_x11ColorBuffer);
    if (this->m_x11Framebuffer != 0)
	glDeleteFramebuffers (1, &this->m_x11Framebuffer);

    glGenFramebuffers (1, &this->m_x11Framebuffer);
    glBindFramebuffer (GL_FRAMEBUFFER, this->m_x11Framebuffer);
    glGenRenderbuffers (1, &this->m_x11ColorBuffer);
    glBindRenderbuffer (GL_RENDERBUFFER, this->m_x11ColorBuffer);
    glRenderbufferStorage (GL_RENDERBUFFER, GL_RGBA8, size.x, size.y);
    glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, this->m_x11ColorBuffer);
    glDrawBuffer (GL_COLOR_ATTACHMENT0);
    glReadBuffer (GL_COLOR_ATTACHMENT0);

    if (glCheckFramebufferStatus (GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	sLog.exception ("Cannot create the X11 composition framebuffer at ", size.x, "x", size.y);

    this->m_x11RenderTargetSize = size;
    this->getApp ().setDestinationFramebuffer (this->m_x11Framebuffer);

    glBindRenderbuffer (GL_RENDERBUFFER, static_cast<GLuint> (previousRenderbuffer));
    glBindFramebuffer (GL_FRAMEBUFFER, static_cast<GLuint> (previousFramebuffer));

    sLog.out ("X11 composition framebuffer: ", size.x, "x", size.y);
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
    const bool useX11Readback = this->m_output->haveImageBuffer ();
    // get the start time of the frame
    startTime = this->getRenderTime ();

    if (useX11Readback) {
	if (this->m_x11Framebuffer == 0) {
	    this->ensureX11RenderTargetSize ({ this->m_output->getFullWidth (), this->m_output->getFullHeight () });
	}
	glBindFramebuffer (GL_FRAMEBUFFER, this->m_x11Framebuffer);
	glDrawBuffer (GL_COLOR_ATTACHMENT0);
    } else {
	glBindFramebuffer (GL_FRAMEBUFFER, 0);
    }

    // clear the screen
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (const auto& [screen, viewport] : this->m_output->getViewports ()) {
	this->getApp ().update (viewport);
    }

    // read the full texture into the image
    if (useX11Readback) {
	const int fullWidth = this->m_output->getFullWidth ();
	const int fullHeight = this->m_output->getFullHeight ();

	glBindFramebuffer (GL_READ_FRAMEBUFFER, this->m_x11Framebuffer);
	glReadBuffer (GL_COLOR_ATTACHMENT0);

	GLint previousPackRowLength = 0;
	glGetIntegerv (GL_PACK_ROW_LENGTH, &previousPackRowLength);
	glPixelStorei (GL_PACK_ROW_LENGTH, fullWidth);

	char* imageBuffer = static_cast<char*> (this->m_output->getImageBuffer ());
	const size_t imageBufferSize = this->m_output->getImageBufferSize ();
	for (const auto& [screen, viewport] : this->m_output->getViewports ()) {
	    const int x = viewport->viewport.x;
	    const int y = viewport->viewport.y;
	    const int width = viewport->viewport.z;
	    const int height = viewport->viewport.w;
	    const size_t offset = (static_cast<size_t> (y) * fullWidth + x) * 4;
	    if (offset >= imageBufferSize)
		sLog.exception ("X11 viewport lies outside the composition image buffer");

	    if (GLEW_VERSION_4_5) {
		glReadnPixels (
		    x, y, width, height, GL_BGRA, GL_UNSIGNED_BYTE, imageBufferSize - offset, imageBuffer + offset
		);
	    } else {
		glReadPixels (x, y, width, height, GL_BGRA, GL_UNSIGNED_BYTE, imageBuffer + offset);
	    }
	}

	glPixelStorei (GL_PACK_ROW_LENGTH, previousPackRowLength);
	glBindFramebuffer (GL_FRAMEBUFFER, 0);

	GLenum error = glGetError ();

	if (error != GL_NO_ERROR) {
	    sLog.exception (
		"OpenGL error when reading the X11 composition framebuffer ", error, " (", fullWidth, "x",
		fullHeight, ")"
	    );
	}
    }

    // TODO: FRAMETIME CONTROL SHOULD GO BACK TO THE CWALLPAPAERAPPLICATION ONCE ACTUAL PARTICLES ARE IMPLEMENTED
    // TODO: AS THOSE, MORE THAN LIKELY, WILL REQUIRE OF A DIFFERENT PROCESSING RATE
    // update the output with the given image
    this->m_output->updateRender ();
    // X11 backgrounds are presented with XPutImage; their tiny hidden GLX
    // drawable never needs to be swapped.
    if (!useX11Readback)
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
