#include "Environment.h"

#include "WallpaperEngine/Logging/Log.h"

#include <GLFW/glfw3.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <algorithm>
#include <ranges>

using namespace WallpaperEngine;
using namespace WallpaperEngine::Desktop::X11;

static void* get_proc_address (void* user_parameter, const char* name) {
	return reinterpret_cast<void*> (glfwGetProcAddress (name));
}

static float get_time (void* user_parameter) { return glfwGetTime (); }

void CustomXIOErrorExitHandler (Display* dsp, void* userdata) {
	const auto context = static_cast<Environment*> (userdata);

	sLog.debugerror ("Critical XServer error detected. Attempting to recover...");

	context->detectOutputs ();
}

int CustomXErrorHandler (Display* dpy, XErrorEvent* event) {
	sLog.debugerror ("Detected X error");

	return 0;
}

int CustomXIOErrorHandler (Display* dsp) {
	sLog.debugerror ("Detected X error");

	return 0;
}

Environment::Environment (
	Application::ApplicationContext& context, ScreenAvailableNotification& availableNotification,
	ScreenUnavailableNotification& unavailableNotification
) : Desktop::Environment (context, availableNotification, unavailableNotification) {
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

#ifdef HAVE_XSETIOERROREXITHANDLER
	XSetIOErrorExitHandler (this->m_display, CustomXIOErrorExitHandler, this);
#endif /* HAVE_XSETIOERROREXITHANDLER */

	this->m_root = DefaultRootWindow (this->m_display);

	XSetErrorHandler (CustomXErrorHandler);
	XSetIOErrorHandler (CustomXIOErrorHandler);

	// setup screen events so screen changes are detected
	XRRSelectInput (this->m_display, this->m_root, RRScreenChangeNotifyMask);
	XSelectInput (this->m_display, this->m_root, SubstructureNotifyMask | PropertyChangeMask);
	this->m_prop_root = XInternAtom (this->m_display, "_XROOTPMAP_ID", False);
	this->m_prop_esetroot = XInternAtom (this->m_display, "ESETROOT_PMAP_ID", False);
	this->m_net_wm_state = XInternAtom (this->m_display, "_NET_WM_STATE", False);
	this->m_net_wm_state_fullscreen = XInternAtom (this->m_display, "_NET_WM_STATE_FULLSCREEN", False);

	this->detectOutputs ();
}

Environment::~Environment () {
	if (this->m_window) {
		glfwDestroyWindow (this->m_window);
	}

	glfwTerminate ();

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
	for (const auto output : this->m_outputs | std::views::values) {
		delete output;
	}

	this->m_outputs.clear ();
}

void Environment::render () {
	for (const auto& output : this->m_outputs | std::views::values) {
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
	XChangeProperty (
		this->m_display, this->m_root, this->m_prop_root, XA_PIXMAP, 32, PropModeReplace,
		reinterpret_cast<unsigned char*> (&this->m_pixmap), 1
	);
	XChangeProperty (
		this->m_display, this->m_root, this->m_prop_esetroot, XA_PIXMAP, 32, PropModeReplace,
		reinterpret_cast<unsigned char*> (&this->m_pixmap), 1
	);

	XClearWindow (this->m_display, this->m_root);
	XFlush (this->m_display);
}

void Environment::detectFullscreen () {
	XEvent ev;
	XWindowAttributes attribs;
	unsigned long nitems, bytes_after;
	Atom* props = nullptr;
	Atom actual_type;
	int actual_format;

	// handle X11 events
	while (XPending (this->m_display)) {
		XNextEvent (this->m_display, &ev);

		if (ev.type == this->m_xrandrEventBase + RRScreenChangeNotify) {
			XRRUpdateConfiguration (&ev);
			this->detectOutputs ();
			continue;
		}

		switch (ev.type) {
			case ConfigureNotify:
				{
					if (ev.xconfigure.window == DefaultRootWindow (this->m_display)) {
						continue;
					}

					const auto it = std::ranges::find (this->m_fullscreenWindowsByGeometry, ev.xconfigure.window);

					XGetWindowAttributes (this->m_display, ev.xconfigure.window, &attribs);
					glm::vec4 viewport (attribs.x, attribs.y, attribs.width, attribs.height);
					bool anyOutput = false;

					// compare it against any of the outputs that are available
					for (const auto output : this->m_outputs | std::views::values) {
						anyOutput = anyOutput || output->getViewport () == viewport;
					}

					if (anyOutput) {
						if (it == this->m_fullscreenWindowsByGeometry.end ()) {
							this->m_fullscreenWindowsByGeometry.push_back (ev.xconfigure.window);
						}
					} else {
						std::erase (this->m_fullscreenWindowsByGeometry, ev.xconfigure.window);
					}
				}
				break;

			case UnmapNotify:
				std::erase (this->m_fullscreenWindowsByGeometry, ev.xunmap.window);
				std::erase (this->m_fullscreenWindowsByState, ev.xunmap.window);
				break;

			case DestroyNotify:
				std::erase (this->m_fullscreenWindowsByGeometry, ev.xdestroywindow.window);
				std::erase (this->m_fullscreenWindowsByState, ev.xdestroywindow.window);
				break;

			case PropertyNotify:
				{
					if (ev.xproperty.atom != this->m_net_wm_state) {
						continue;
					}

					if (ev.xproperty.window == DefaultRootWindow (this->m_display)) {
						continue;
					}

					if (!XGetWindowProperty (
							this->m_display, ev.xproperty.window, this->m_net_wm_state, 0, 32, False, XA_ATOM,
							&actual_type, &actual_format, &nitems, &bytes_after,
							reinterpret_cast<unsigned char**> (&props)
						)) {
						continue;
					}

					for (unsigned long i = 0; i < nitems; i++) {
						if (props[i] != this->m_net_wm_state_fullscreen) {
							continue;
						}

						std::erase (this->m_fullscreenWindowsByState, ev.xproperty.window);
					}

					if (props) {
						XFree (props);
						props = nullptr;
					}
				}
				break;
		}
	}

	this->anything_fullscreen
		= !this->m_fullscreenWindowsByGeometry.empty () || !this->m_fullscreenWindowsByState.empty ();
}

uint64_t Environment::getCurrentFrame () { return this->m_frameCount; }

bool Environment::isCloseRequested () { return glfwWindowShouldClose (this->m_window); }

void Environment::registerOutput (const std::string& name, const glm::vec4 viewport) {
	auto output = new Output (nullptr, name, viewport);

	this->m_outputs.insert_or_assign (name, output);

	this->onScreenAvailable (name, output);
}

void Environment::deregisterOutput (Output* output) {
	this->onScreenUnavailable (output->name, output);
	this->m_outputs.erase (output->name);
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
	for (const auto& output : this->m_outputs | std::views::values) {
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

		const auto it = this->m_outputs.find (info->name);

		if (it == this->m_outputs.end ()) {
			this->registerOutput (info->name, glm::vec4 (crtc->x, crtc->y, crtc->width, crtc->height));
		} else {
			it->second->setViewport (glm::vec4 (crtc->x, crtc->y, crtc->width, crtc->height));
		}

		outputs.emplace_back (info->name);

		XRRFreeCrtcInfo (crtc);
	}

	XRRFreeScreenResources (resources);

	this->updatePixmap ();

	// find not-present outputs
	std::vector<Output*> toRemove;

	for (const auto& [name, output] : this->m_outputs) {
		if (std::ranges::find (outputs, name) == outputs.end ()) {
			toRemove.emplace_back (output);
		}
	}

	for (const auto output : toRemove) {
		this->deregisterOutput (output);
	}
}