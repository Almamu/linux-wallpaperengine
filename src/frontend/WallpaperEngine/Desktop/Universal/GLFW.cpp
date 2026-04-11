#include "GLFW.h"

#include "WallpaperEngine/Logging/Log.h"
#include "glad/glad.h"

#include <GLFW/glfw3.h>

using namespace WallpaperEngine::Desktop::Universal;
using namespace WallpaperEngine::Application;

void CustomGLFWErrorHandler (int errorCode, const char* reason) { sLog.error ("GLFW error ", errorCode, ": ", reason); }

float get_time (void* user_parameter) { return glfwGetTime (); }

void* get_proc_address (void* user_parameter, const char* name) {
	return reinterpret_cast<void*> (glfwGetProcAddress (name));
}

GLFW::GLFW (ApplicationContext& context) :
	m_context (context), m_output (nullptr, { 0, 0, 640, 480 }), m_framecount (0) {
	glfwSetErrorCallback (CustomGLFWErrorHandler);

	if (glfwInit () == GLFW_FALSE) {
		sLog.exception ("Failed to initialize GLFW");
	}

	this->counter = { .user_parameter = this, .get_time = get_time };

	glfwWindowHint (GLFW_SAMPLES, 4);
	glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	// set x11-specific hints
	glfwWindowHintString (GLFW_X11_CLASS_NAME, "linux-wallpaperengine");
	glfwWindowHintString (GLFW_X11_INSTANCE_NAME, "linux-wallpaperengine");

	// for forced window mode setting some hints disables borders, etc
	if (this->m_context.settings.render.mode == ApplicationContext::EXPLICIT_WINDOW) {
		glfwWindowHint (GLFW_DECORATED, GLFW_FALSE);
		glfwWindowHint (GLFW_RESIZABLE, GLFW_FALSE);
		glfwWindowHint (GLFW_FLOATING, GLFW_TRUE);
	}

#if !NDEBUG
	glfwWindowHint (GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

	this->m_window = glfwCreateWindow (640, 480, "", nullptr, nullptr);

	if (this->m_window == nullptr) {
		sLog.exception ("Failed to create GLFW window");
	}

	if (this->m_context.settings.render.mode == ApplicationContext::EXPLICIT_WINDOW) {
		// TODO: ADD SUPPORT FOR MULTIPLE WINDOWS!
		glfwSetWindowPos (
			this->m_window, this->m_context.settings.render.window.geometry.x,
			this->m_context.settings.render.window.geometry.y
		);
		glfwSetWindowSize (
			this->m_window, this->m_context.settings.render.window.geometry.z,
			this->m_context.settings.render.window.geometry.w
		);
	}

	glfwMakeContextCurrent (this->m_window);

	// initalize glad
	if (!gladLoadGLLoader (reinterpret_cast<GLADloadproc> (glfwGetProcAddress))) {
		sLog.exception ("Failed to initialize glad");
	}

	// finally set the right gl_proc_address calls
	this->gl_proc_address = { .user_parameter = this, .get_proc_address = get_proc_address };
}

GLFW::~GLFW () {
	if (this->m_window) {
		glfwDestroyWindow (this->m_window);
	}

	glfwTerminate ();
}

void GLFW::render () {
	int width;
	int height;

	glfwGetFramebufferSize (this->m_window, &width, &height);

	this->m_output.setViewport ({ 0, 0, width, height });
	this->m_output.render ();

	glfwSwapBuffers (this->m_window);
	glfwPollEvents ();
}

void GLFW::detectFullscreen () {
	// glfw does not support fullscreen detection for now
	// in the future this should be separated from the render
	// but should be more than enough for now
}

uint64_t GLFW::getCurrentFrame () { return this->m_framecount; }

bool GLFW::isCloseRequested () { return glfwWindowShouldClose (this->m_window); }

WallpaperEngine::Desktop::Output* GLFW::requestOutput (const std::string& name) {
	if (name.compare ("default") != 0) {
		sLog.exception ("GLFW does not support multiple outputs, only default available");
	}

	return &this->m_output;
}

WallpaperEngine::Desktop::Output* GLFW::getOutput (const std::string& name) { return &this->m_output; }
