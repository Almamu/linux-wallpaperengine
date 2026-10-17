#include "X11FullScreenDetector.h"
#include "WallpaperEngine/Logging/Log.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>

#include "WallpaperEngine/Render/Drivers/GLFWOpenGLDriver.h"
#include "WallpaperEngine/Render/Drivers/VideoFactories.h"

namespace WallpaperEngine::Render::Drivers::Detectors {
void CustomXIOErrorExitHandler (Display* dsp, void* userdata) {
    const auto context = static_cast<X11FullScreenDetector*> (userdata);

    sLog.debugerror ("Critical XServer error detected. Attempting to recover...");

    // refetch all the resources
    context->reset ();
}

int CustomXErrorHandler (Display* dpy, XErrorEvent* event) {
    sLog.debugerror ("Detected X error");

    return 0;
}

int CustomXIOErrorHandler (Display* dsp) {
    sLog.debugerror ("Detected X error");

    return 0;
}

X11FullScreenDetector::X11FullScreenDetector (Application::ApplicationContext& appContext, VideoDriver& driver) :
    FullScreenDetector (appContext), m_display (nullptr), m_root (0), m_driver (driver) {
    try {
	// attempt casting to CGLFWOpenGLDriver, this will throw if it's not possible
	// so we can gracely handle the error
	std::ignore = dynamic_cast<GLFWOpenGLDriver&> (this->m_driver);
    } catch (std::exception&) {
	sLog.exception ("X11 FullScreen Detector initialized with the wrong video driver... This is a bug...");
    }

    // do not use previous handler, it might stop the app under weird circumstances
    // these handlers might be replaced by other X11-specific functionality, they
    // should only be used to ignore X11 errors and nothing else
    // so this doesn't affect functionality
    XSetErrorHandler (CustomXErrorHandler);
    XSetIOErrorHandler (CustomXIOErrorHandler);

    this->initialize ();
}

X11FullScreenDetector::~X11FullScreenDetector () { this->stop (); }

bool X11FullScreenDetector::anythingFullscreen () const {
    // Stop rendering when another visible window is maximized or covers an output.
    bool isFullscreen = false;
    XWindowAttributes attribs;
    Window _;
    Window* children;
    unsigned int nchildren;
    const Atom stateAtom = XInternAtom (this->m_display, "_NET_WM_STATE", True);
    const Atom maximizedVertAtom = XInternAtom (this->m_display, "_NET_WM_STATE_MAXIMIZED_VERT", True);
    const Atom maximizedHorzAtom = XInternAtom (this->m_display, "_NET_WM_STATE_MAXIMIZED_HORZ", True);

    const auto isMaximized = [&] (const Window window) {
	if (stateAtom == None || maximizedVertAtom == None || maximizedHorzAtom == None) {
	    return false;
	}
	Atom actualType = None;
	int actualFormat = 0;
	unsigned long itemCount = 0;
	unsigned long bytesAfter = 0;
	unsigned char* property = nullptr;
	const auto result = XGetWindowProperty (
	    this->m_display, window, stateAtom, 0, 1024, False, XA_ATOM, &actualType, &actualFormat,
	    &itemCount, &bytesAfter, &property
	);
	bool maximizedVert = false;
	bool maximizedHorz = false;
	if (result == Success && actualType == XA_ATOM && actualFormat == 32 && property) {
	    const auto states = reinterpret_cast<const Atom*> (property);
	    for (unsigned long stateIndex = 0; stateIndex < itemCount; ++stateIndex) {
		maximizedVert = maximizedVert || states[stateIndex] == maximizedVertAtom;
		maximizedHorz = maximizedHorz || states[stateIndex] == maximizedHorzAtom;
	    }
	}
	if (property) {
	    XFree (property);
	}
	return maximizedVert && maximizedHorz;
    };

    if (!XQueryTree (this->m_display, this->m_root, &_, &_, &children, &nchildren)) {
	return false;
    }

    const auto ourWindow = reinterpret_cast<Window> (dynamic_cast<GLFWOpenGLDriver&> (this->m_driver).getWindow ());
    Window parentWindow;

    {
	Window root, *schildren = nullptr;
	unsigned int num_children;

	if (!XQueryTree (this->m_display, ourWindow, &root, &parentWindow, &schildren, &num_children)) {
	    XFree (children);
	    return false;
	}

	if (schildren) {
	    XFree (schildren);
	}
    }

    for (unsigned int i = 0; i < nchildren; i++) {
	if (!XGetWindowAttributes (this->m_display, children[i], &attribs)) {
	    continue;
	}

	// ignore ourselves
	if (ourWindow == children[i] || parentWindow == children[i]) {
	    continue;
	}

	if (attribs.map_state != IsViewable) {
	    continue;
	}

	if (isMaximized (children[i])) {
	    isFullscreen = true;
	    break;
	}
	// Reparenting window managers keep _NET_WM_STATE on the client child.
	Window childRoot, childParent;
	Window* clientWindows = nullptr;
	unsigned int clientWindowCount = 0;
	if (XQueryTree (this->m_display, children[i], &childRoot, &childParent, &clientWindows, &clientWindowCount)) {
	    for (unsigned int clientIndex = 0; clientIndex < clientWindowCount; ++clientIndex) {
		if (isMaximized (clientWindows[clientIndex])) {
		    isFullscreen = true;
		    break;
		}
	    }
	    if (clientWindows) {
		XFree (clientWindows);
	    }
	}
	if (isFullscreen) {
	    break;
	}

	// compare width and height with the different screens we have
	for (const auto& [name, viewport] : this->m_screens) {
	    if (attribs.x == viewport.x && attribs.y == viewport.y && attribs.width == viewport.z
		&& attribs.height == viewport.w) {
		isFullscreen = true;
		break;
	    }
	}
    }

    XFree (children);

    return isFullscreen;
}

void X11FullScreenDetector::reset () {
    this->stop ();
    this->initialize ();
}

void X11FullScreenDetector::initialize () {
    this->m_display = XOpenDisplay (nullptr);

    // set the error handling to try and recover from X disconnections
#ifdef HAVE_XSETIOERROREXITHANDLER
    XSetIOErrorExitHandler (this->m_display, CustomXIOErrorExitHandler, this);
#endif /* HAVE_XSETIOERROREXITHANDLER */

    int xrandr_result, xrandr_error;

    if (!XRRQueryExtension (this->m_display, &xrandr_result, &xrandr_error)) {
	sLog.error ("XRandr is not present, fullscreen detection might not work");
	return;
    }

    this->m_root = DefaultRootWindow (this->m_display);
    XRRScreenResources* screenResources = XRRGetScreenResources (this->m_display, this->m_root);

    if (screenResources == nullptr) {
	sLog.error ("Cannot detect screen sizes using xrandr, fullscreen detection might not work");
	return;
    }

    for (int i = 0; i < screenResources->noutput; i++) {
	const XRROutputInfo* info = XRRGetOutputInfo (this->m_display, screenResources, screenResources->outputs[i]);

	// screen not in use, ignore it
	if (info == nullptr || info->connection != RR_Connected) {
	    continue;
	}

	XRRCrtcInfo* crtc = XRRGetCrtcInfo (this->m_display, screenResources, info->crtc);

	// screen not active, ignore it
	if (crtc == nullptr) {
	    continue;
	}

	// add the screen to the list of screens
	this->m_screens.emplace (std::string (info->name), glm::ivec4 (crtc->x, crtc->y, crtc->width, crtc->height));

	XRRFreeCrtcInfo (crtc);
    }

    XRRFreeScreenResources (screenResources);
}

void X11FullScreenDetector::stop () {
    if (this->m_display == nullptr) {
	return;
    }

    XCloseDisplay (this->m_display);
    this->m_display = nullptr;
}

__attribute__ ((constructor)) void registerX11FullscreenDetector () {
    sVideoFactories.registerFullscreenDetector (
	"x11", [] (ApplicationContext& context, VideoDriver& driver) -> std::unique_ptr<FullScreenDetector> {
	    return std::make_unique<X11FullScreenDetector> (context, driver);
	}
    );
}

} // namespace WallpaperEngine::Render::Drivers::Detectors
