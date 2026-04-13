#include "Environment.h"

#include "WallpaperEngine/Logging/Log.h"

#include <GLFW/glfw3.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <algorithm>
#include <ranges>

using namespace WallpaperEngine;
using namespace WallpaperEngine::Desktop::X11;

static void* get_proc_address (void* user_parameter, const char* name) {
	return reinterpret_cast<void*> (glfwGetProcAddress (name));
}

static float get_time (void* user_parameter) { return glfwGetTime (); }

Environment::Environment (Application::ApplicationContext& context) : Desktop::Environment (context) {
	this->m_closeRequested = false;
	this->m_framebuffer = GL_NONE;
	this->m_texture = GL_NONE;
	this->counter = {
		.user_parameter = this,
		.get_time = get_time,
	};
	this->gl_proc_address = {
		.user_parameter = this,
		.get_proc_address = get_proc_address,
	};
	this->mouse_input = {
		.user_parameter = this,
		.get_x = nullptr,
		.get_y = nullptr,
		.is_pressed = nullptr,
	};
	// TODO: MOUSE INPUT SUPPORT!

	// initialize glfw first
	if (glfwInit () == GLFW_FALSE) {
		sLog.exception ("Failed to initialize GLFW");
	}

	glfwWindowHint (GLFW_SAMPLES, 4);
	glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint (GLFW_VISIBLE, GLFW_FALSE);
	// set X11-specific hints
	glfwWindowHintString (GLFW_X11_CLASS_NAME, "linux-wallpaperengine");
	glfwWindowHintString (GLFW_X11_INSTANCE_NAME, "linux-wallpaperengine");

#if !NDEBUG
	glfwWindowHint (GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

	// create a window, doesn't matter size because it'll be invisible to users
	this->m_window = glfwCreateWindow (640, 480, "linux-wallpaperengine", nullptr, nullptr);

	if (this->m_window == nullptr) {
		sLog.exception ("Failed to create GLFW window");
	}

	glfwMakeContextCurrent (this->m_window);

	// initialize GLAD
	if (!gladLoadGLLoader (reinterpret_cast<GLADloadproc> (glfwGetProcAddress))) {
		sLog.exception ("Failed to initialize GLAD");
	}

	this->m_display = XOpenDisplay (nullptr);
	// TODO: set some error handling to try and recover from X disconnections
	int xrandr_error;

	if (!XRRQueryExtension (this->m_display, &this->m_xrandrEventBase, &xrandr_error)) {
		sLog.error ("XRandr is not present");
	}

	this->m_root = DefaultRootWindow (this->m_display);

	// setup screen events so screen changes are detected
	XRRSelectInput (this->m_display, this->m_root, RRScreenChangeNotifyMask);

	this->detectOutputs ();
}

Environment::~Environment () {
	if (this->m_framebuffer != GL_NONE) {
		glDeleteFramebuffers (1, &this->m_framebuffer);
		this->m_framebuffer = GL_NONE;
	}

	if (this->m_texture != GL_NONE) {
		glDeleteTextures (1, &this->m_texture);
		this->m_texture = GL_NONE;
	}

	if (this->m_image) {
		XDestroyImage (this->m_image);
		this->m_image = nullptr;
	}

	if (this->m_gc) {
		XFreeGC (this->m_display, this->m_gc);
		this->m_gc = nullptr;
	}

	if (this->m_pixmap) {
		XFreePixmap (this->m_display, this->m_pixmap);
		this->m_pixmap = 0;
	}

	if (this->m_display != nullptr) {
		XCloseDisplay (this->m_display);
		this->m_display = nullptr;
	}

	// free all outputs
	for (const auto output : this->m_requestedOutputs | std::views::values) {
		delete output;
	}

	for (const auto output : this->m_outputs) {
		delete output;
	}

	this->m_requestedOutputs.clear ();
	this->m_outputs.clear ();
}

void Environment::render () {
	for (const auto& output : this->m_requestedOutputs | std::views::values) {
		output->render ();
	}

	glfwSwapBuffers (this->m_window);
	glfwPollEvents ();

	this->m_frameCount++;

	// read pixel info into the image data buffer
	glReadPixels (0, 0, this->m_fullWidth, this->m_fullHeight, GL_BGRA, GL_UNSIGNED_BYTE, this->m_imageData);

	// put the image back into the screen
	XPutImage (
		this->m_display, this->m_pixmap, this->m_gc, this->m_image, 0, 0, 0, 0, this->m_fullWidth, this->m_fullHeight
	);

	// _XROOTPMAP_ID & ESETROOT_PMAP_ID allow other program (compositors) to
	// edit the background. Without these, other programs will clear the screen.
	// it also forces the compositor to refresh the background (tested with picom)
	const Atom prop_root = XInternAtom (this->m_display, "_XROOTPMAP_ID", False);
	const Atom prop_esetroot = XInternAtom (this->m_display, "ESETROOT_PMAP_ID", False);
	XChangeProperty (
		this->m_display, this->m_root, prop_root, XA_PIXMAP, 32, PropModeReplace,
		reinterpret_cast<unsigned char*> (&this->m_pixmap), 1
	);
	XChangeProperty (
		this->m_display, this->m_root, prop_esetroot, XA_PIXMAP, 32, PropModeReplace,
		reinterpret_cast<unsigned char*> (&this->m_pixmap), 1
	);

	XClearWindow (this->m_display, this->m_root);
	XFlush (this->m_display);
}

void Environment::detectFullscreen () {
	XEvent ev;

	// handle X11 events
	while (XPending (this->m_display)) {
		XNextEvent (this->m_display, &ev);

		if (ev.type != this->m_xrandrEventBase + RRScreenChangeNotify) {
			continue;
		}

		XRRUpdateConfiguration (&ev);
		this->detectOutputs ();
	}
}

uint64_t Environment::getCurrentFrame () { return this->m_frameCount; }

bool Environment::isCloseRequested () { return glfwWindowShouldClose (this->m_window); }

void Environment::registerOutput (const std::string& name, const glm::vec4 viewport) {
	auto output = new Output (nullptr, name, viewport);

	this->m_outputs.push_back (output);

	const auto it = this->m_requestedOutputs.find (name);

	if (it == this->m_requestedOutputs.end ()) {
		return;
	}

	it->second->setRealOutput (output);
	it->second->setViewport (viewport);
}

void Environment::deregisterOutput (Output* output) {
	const auto it = this->m_requestedOutputs.find (output->name);

	if (it != this->m_requestedOutputs.end ()) {
		it->second->setRealOutput (nullptr);
	}

	std::erase (this->m_outputs, output);
}

Desktop::Output* Environment::requestOutput (const std::string& name) {
	// register the virtual output
	if (this->m_requestedOutputs.contains (name)) {
		sLog.exception ("Requested output ", name, " was already requested");
	}

	// check for a matching real output (if any)
	const auto realOutput
		= std::ranges::find_if (this->m_outputs, [&name] (const Output* output) { return output->name == name; });

	auto newOutput = new VirtualOutput (realOutput == this->m_outputs.end () ? nullptr : *realOutput);

	this->m_requestedOutputs.emplace (name, newOutput);

	return newOutput;
}

Desktop::Output* Environment::getOutput (const std::string& name) {
	const auto it = this->m_requestedOutputs.find (name);

	if (it == this->m_requestedOutputs.end ()) {
		sLog.exception ("Requested output ", name, " was not found");
	}

	return it->second;
}

void Environment::updatePixmap () {
	if (this->m_framebuffer != GL_NONE) {
		glDeleteFramebuffers (1, &this->m_framebuffer);
		this->m_framebuffer = GL_NONE;
	}

	if (this->m_texture != GL_NONE) {
		glDeleteTextures (1, &this->m_texture);
		this->m_texture = GL_NONE;
	}

	if (this->m_image) {
		XDestroyImage (this->m_image);
		this->m_image = nullptr;
	}

	if (this->m_gc) {
		XFreeGC (this->m_display, this->m_gc);
		this->m_gc = nullptr;
	}

	if (this->m_pixmap) {
		XFreePixmap (this->m_display, this->m_pixmap);
		this->m_pixmap = 0;
	}

	this->m_fullWidth = DisplayWidth (this->m_display, DefaultScreen (this->m_display));
	this->m_fullHeight = DisplayHeight (this->m_display, DefaultScreen (this->m_display));

	this->m_pixmap = XCreatePixmap (this->m_display, this->m_root, this->m_fullWidth, this->m_fullHeight, 24);
	this->m_gc = XCreateGC (this->m_display, this->m_pixmap, 0, nullptr);
	// prefill the pixmap with black
	XFillRectangle (this->m_display, this->m_pixmap, this->m_gc, 0, 0, this->m_fullWidth, this->m_fullHeight);
	// set the window background as our pixmap
	XSetWindowBackgroundPixmap (this->m_display, this->m_root, this->m_pixmap);
	this->m_imageSize = this->m_fullWidth * this->m_fullHeight * 4;
	this->m_imageData = new char[this->m_imageSize];
	// create an image so we can copy it over
	this->m_image = XCreateImage (
		this->m_display, CopyFromParent, 24, ZPixmap, 0, this->m_imageData, this->m_fullWidth, this->m_fullHeight, 32, 0
	);

	glGenTextures (1, &this->m_texture);
	glGenFramebuffers (1, &this->m_framebuffer);

	glBindFramebuffer (GL_FRAMEBUFFER, this->m_framebuffer);

	glBindTexture (GL_TEXTURE_2D, this->m_texture);
	glTexImage2D (
		GL_TEXTURE_2D, 0, GL_RGBA8, this->m_fullWidth, this->m_fullHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr
	);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	constexpr GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
	glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->m_texture, 0);
	glDrawBuffers (1, drawBuffers);

	if (glCheckFramebufferStatus (GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		sLog.exception ("Framebuffer setup failed");
	}

	glClear (GL_COLOR_BUFFER_BIT);

	// update all the outputs with the new framebuffer to be used
	for (const auto& output : this->m_requestedOutputs | std::views::values) {
		output->setFramebuffer (this->m_framebuffer);
	}
}

void Environment::detectOutputs () {
	XRRScreenResources* resources = XRRGetScreenResources (this->m_display, DefaultRootWindow (this->m_display));

	if (resources == nullptr) {
		sLog.exception ("Cannot detect screen sizes using xrandr");
	}

	std::vector<std::string> outputs;

	for (int i = 0; i < resources->noutput; i++) {
		const auto info = XRRGetOutputInfo (this->m_display, resources, resources->outputs[i]);

		if (info == nullptr || info->connection != RR_Connected) {
			continue;
		}

		auto crtc = XRRGetCrtcInfo (this->m_display, resources, info->crtc);

		if (crtc == nullptr) {
			continue;
		}

		const auto it = std::ranges::find_if (this->m_outputs, [&info] (const Output* output) {
			return output->name == info->name;
		});

		if (it == this->m_outputs.end ()) {
			this->registerOutput (info->name, glm::vec4 (crtc->x, crtc->y, crtc->width, crtc->height));
		} else {
			// output already exists, update the viewport
			const auto requestedIt = this->m_requestedOutputs.find (info->name);

			if (requestedIt != this->m_requestedOutputs.end ()) {
				requestedIt->second->setViewport (glm::vec4 (crtc->x, crtc->y, crtc->width, crtc->height));
				requestedIt->second->setRealOutput (*it);
			} else {
				(*it)->setViewport (glm::vec4 (crtc->x, crtc->y, crtc->width, crtc->height));
			}
		}

		outputs.emplace_back (info->name);

		XRRFreeCrtcInfo (crtc);
	}

	XRRFreeScreenResources (resources);

	this->updatePixmap ();

	// find not-present outputs
	std::vector<Output*> toRemove;

	std::ranges::copy_if (this->m_outputs, toRemove.begin (), [outputs] (const Output* output) {
		return std::ranges::find (outputs, output->name) == outputs.end ();
	});

	for (const auto& output : toRemove) {
		this->deregisterOutput (output);
	}
}