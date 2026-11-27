#include "X11FullScreenDetector.h"
#include "WallpaperEngine/Logging/Log.h"

#include <algorithm>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include "WallpaperEngine/Render/Drivers/GLFWOpenGLDriver.h"
#include "WallpaperEngine/Render/Drivers/VideoFactories.h"

#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

namespace WallpaperEngine::Render::Drivers::Detectors {
namespace {
struct WindowTree {
    Window root = None, parent = None;
    unsigned int count = 0;
    std::unique_ptr<Window[], decltype (&XFree)> children { nullptr, XFree };

    bool query (Display* display, Window window) {
	Window* result = nullptr;
	const bool success = XQueryTree (display, window, &root, &parent, &result, &count);
	children.reset (result);
	return success;
    }
};

Atom windowType (Display* display, Window window, Atom property) {
    Atom type = None;
    int format = 0;
    unsigned long count = 0, remaining = 0;
    unsigned char* value = nullptr;
    const int status = XGetWindowProperty (
	display, window, property, 0, 1, False, XA_ATOM, &type, &format, &count, &remaining, &value
    );
    const std::unique_ptr<unsigned char, decltype (&XFree)> data { value, XFree };
    return status == Success && type == XA_ATOM && format == 32 && count == 1 && data
	? *reinterpret_cast<const Atom*> (data.get ()) : None;
}

bool isDesktopWindow (Display* display, Window window, Atom property, Atom desktopType) {
    const Atom type = windowType (display, window, property);
    if (type != None) return type == desktopType;

    // Reparenting window managers attach the role to the client inside their frame.
    WindowTree frame;
    if (!frame.query (display, window)) return false;
    for (unsigned int i = 0; i < frame.count; ++i) {
	const Window child = frame.children[i];
	XWindowAttributes attributes;
	if (XGetWindowAttributes (display, child, &attributes) && attributes.map_state == IsViewable
	    && windowType (display, child, property) == desktopType) return true;
    }
    return false;
}
} // namespace

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
    FullScreenDetector (appContext), m_driver (driver) {
    this->initialize ();
}

X11FullScreenDetector::~X11FullScreenDetector () { this->stop (); }

bool X11FullScreenDetector::anythingFullscreen () const {
    if (!this->m_display || this->m_window == None || this->m_screens.empty ()) return false;

    Window ownRootChild = this->m_window;
    while (true) {
	WindowTree ownTree;
	if (!ownTree.query (this->m_display, ownRootChild) || ownTree.root != this->m_root
	    || ownTree.parent == None) return false;
	if (ownTree.parent == this->m_root) break;
	ownRootChild = ownTree.parent;
    }

    WindowTree root;
    if (!root.query (this->m_display, this->m_root)) return false;
    for (unsigned int i = 0; i < root.count; ++i) {
	const Window window = root.children[i];
	if (window == ownRootChild) continue;
	XWindowAttributes attributes;
	if (!XGetWindowAttributes (this->m_display, window, &attributes) || attributes.map_state != IsViewable
	    || attributes.c_class != InputOutput) continue;
	const bool coversOutput = std::ranges::any_of (this->m_screens, [&] (const auto& screen) {
	    const auto& viewport = screen.second;
	    return glm::ivec4 (attributes.x, attributes.y, attributes.width, attributes.height) == viewport;
	});
	if (coversOutput && !isDesktopWindow (this->m_display, window, this->m_windowType, this->m_desktopType))
	    return true;
    }
    return false;
}

void X11FullScreenDetector::reset () {
    this->stop ();
    this->initialize ();
}

void X11FullScreenDetector::initialize () {
    const auto* driver = dynamic_cast<const GLFWOpenGLDriver*> (&this->m_driver);
    if (!driver || !driver->getWindow ()) return;
#ifdef GLFW_PLATFORM_X11
    if (glfwGetPlatform () != GLFW_PLATFORM_X11) return;
#endif
    Display* nativeDisplay = glfwGetX11Display ();
    if (!nativeDisplay) return;
    this->m_window = glfwGetX11Window (driver->getWindow ());
    if (this->m_window == None) return;

    this->m_display = XOpenDisplay (DisplayString (nativeDisplay));
    if (!this->m_display) return;

    // A window may disappear between enumeration and the attribute query.
    XSetErrorHandler (CustomXErrorHandler);
    XSetIOErrorHandler (CustomXIOErrorHandler);

    // set the error handling to try and recover from X disconnections
#ifdef HAVE_XSETIOERROREXITHANDLER
    XSetIOErrorExitHandler (this->m_display, CustomXIOErrorExitHandler, this);
#endif /* HAVE_XSETIOERROREXITHANDLER */

    int xrandr_result, xrandr_error;

    if (!XRRQueryExtension (this->m_display, &xrandr_result, &xrandr_error)) {
	sLog.error ("XRandr is not present, fullscreen detection might not work");
	return;
    }

    XWindowAttributes ownAttributes;
    if (!XGetWindowAttributes (this->m_display, this->m_window, &ownAttributes)) return;
    this->m_root = ownAttributes.root;
    this->m_windowType = XInternAtom (this->m_display, "_NET_WM_WINDOW_TYPE", False);
    this->m_desktopType = XInternAtom (this->m_display, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
    XRRScreenResources* screenResources = XRRGetScreenResources (this->m_display, this->m_root);

    if (screenResources == nullptr) {
	sLog.error ("Cannot detect screen sizes using xrandr, fullscreen detection might not work");
	return;
    }

    for (int i = 0; i < screenResources->noutput; i++) {
	const std::unique_ptr<XRROutputInfo, decltype (&XRRFreeOutputInfo)> info {
	    XRRGetOutputInfo (this->m_display, screenResources, screenResources->outputs[i]), XRRFreeOutputInfo
	};

	// screen not in use, ignore it
	if (!info || info->connection != RR_Connected || info->crtc == None) {
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
    this->m_screens.clear ();
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
