#include "X11Output.h"
#include "GLFWOutputViewport.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Render/Drivers/GLFWOpenGLDriver.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <limits>
#include <ranges>

#include <sys/ipc.h>
#include <sys/shm.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/XShm.h>
#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
#endif

using namespace WallpaperEngine::Render::Drivers::Output;

namespace {
thread_local bool trapXErrors = false;
thread_local bool trappedXError = false;

bool supportsBGRAReadback (const XImage* image, int width) {
    return image != nullptr && image->bits_per_pixel == 32
	&& image->bytes_per_line == static_cast<long> (width) * 4 && image->byte_order == LSBFirst
	&& image->red_mask == 0x00FF0000UL && image->green_mask == 0x0000FF00UL
	&& image->blue_mask == 0x000000FFUL;
}

uint32_t checkedImageSize (const XImage* image) {
    const size_t imageBytes = static_cast<size_t> (image->bytes_per_line) * image->height;
    if (imageBytes > std::numeric_limits<uint32_t>::max ())
	sLog.exception ("X11 composition image exceeds the supported buffer size");
    return static_cast<uint32_t> (imageBytes);
}
} // namespace

void CustomXIOErrorExitHandler (Display*, void*) {
    // Xlib invokes this immediately before terminating after a connection loss.
    // The Display is already unusable, so attempting teardown or recovery here
    // would only issue more requests on the failed connection.
    sLog.debugerror ("Critical X server connection error detected; exiting");
}

int CustomXErrorHandler (Display*, XErrorEvent*) {
    if (trapXErrors) {
	trappedXError = true;
	return 0;
    }

    sLog.debugerror ("Detected X error");
    return 0;
}

int CustomXIOErrorHandler (Display*) {
    sLog.debugerror ("Detected X I/O error");
    return 0;
}

X11Output::X11Output (ApplicationContext& context, VideoDriver& driver) : Output (context, driver) {
    XSetErrorHandler (CustomXErrorHandler);
    XSetIOErrorHandler (CustomXIOErrorHandler);
    this->loadScreenInfo ();
}

X11Output::~X11Output () { this->free (); }

void X11Output::reset () {
    this->free ();
    this->loadScreenInfo ();
}

void X11Output::free () {
    for (const auto* screen : this->m_screens)
	delete screen;

    this->m_screens.clear ();
    this->m_viewports.clear ();

    if (this->m_display != nullptr) {
	this->freePerOutputWindows ();
	this->freeImageBuffer ();
	this->freeRootPixmap ();
	XCloseDisplay (this->m_display);
    }

    this->m_windowGCs.clear ();
    this->m_windows.clear ();
    this->m_display = nullptr;
    this->m_pixmap = None;
    this->m_root = None;
    this->m_gc = None;
    this->m_rootPixmapAtom = None;
    this->m_esetrootPixmapAtom = None;
    this->m_imageSize = 0;
    this->m_usePerOutputWindows = false;
    this->m_useShm = false;
    this->m_shmInfo = {};
    this->m_lastRootSync = {};
}

void X11Output::freePerOutputWindows () {
    for (const auto& [name, gc] : this->m_windowGCs) {
	if (gc != None)
	    XFreeGC (this->m_display, gc);
    }

    for (const auto& [name, window] : this->m_windows) {
	if (window != None)
	    XDestroyWindow (this->m_display, window);
    }
}

void X11Output::freeImageBuffer () {
    if (this->m_image == nullptr) {
	std::free (this->m_imageData);
	this->m_imageData = nullptr;
	return;
    }

    if (this->m_useShm) {
	char* shmAddress = this->m_shmInfo.shmaddr;
	XShmDetach (this->m_display, &this->m_shmInfo);
	XSync (this->m_display, False);
	// The shared segment is detached separately; never pass it to free().
	this->m_image->data = nullptr;
	XDestroyImage (this->m_image);
	if (shmAddress != nullptr && shmAddress != reinterpret_cast<char*> (-1))
	    shmdt (shmAddress);
    } else {
	// XDestroyImage owns and frees m_imageData for the fallback image.
	XDestroyImage (this->m_image);
    }

    this->m_image = nullptr;
    this->m_imageData = nullptr;
}

void X11Output::freeRootPixmap () {
    if (this->m_gc != None)
	XFreeGC (this->m_display, this->m_gc);
    if (this->m_pixmap == None)
	return;

    // The root properties must never retain the XID after its pixmap is freed.
    XSetWindowBackground (
	this->m_display, this->m_root, BlackPixel (this->m_display, DefaultScreen (this->m_display))
    );
    if (this->m_rootPixmapAtom != None)
	XDeleteProperty (this->m_display, this->m_root, this->m_rootPixmapAtom);
    if (this->m_esetrootPixmapAtom != None)
	XDeleteProperty (this->m_display, this->m_root, this->m_esetrootPixmapAtom);
    XClearWindow (this->m_display, this->m_root);
    XFreePixmap (this->m_display, this->m_pixmap);
}

void* X11Output::getImageBuffer () const { return this->m_imageData; }

bool X11Output::renderVFlip () const { return false; }

bool X11Output::renderMultiple () const { return this->m_viewports.size () > 1; }

bool X11Output::haveImageBuffer () const { return this->m_imageData != nullptr; }

uint32_t X11Output::getImageBufferSize () const { return this->m_imageSize; }

void X11Output::loadScreenInfo () {
    this->m_display = XOpenDisplay (nullptr);
    if (this->m_display == nullptr)
	sLog.exception ("Cannot open the X11 display");

#ifdef HAVE_XSETIOERROREXITHANDLER
    XSetIOErrorExitHandler (this->m_display, CustomXIOErrorExitHandler, this);
#endif

    int eventBase = 0;
    int errorBase = 0;
    if (!XRRQueryExtension (this->m_display, &eventBase, &errorBase))
	sLog.exception ("XRandR is not available; cannot detect X11 outputs");

    this->m_root = DefaultRootWindow (this->m_display);
    this->m_rootWidth = DisplayWidth (this->m_display, DefaultScreen (this->m_display));
    this->m_rootHeight = DisplayHeight (this->m_display, DefaultScreen (this->m_display));

    XRRScreenResources* resources = XRRGetScreenResources (this->m_display, this->m_root);
    if (resources == nullptr)
	sLog.exception ("Cannot query X11 output sizes with XRandR");

    this->discoverOutputs (resources);
    XRRFreeScreenResources (resources);
    this->validateOutputs ();
    this->initX11Background ();
}

void X11Output::discoverOutputs (XRRScreenResources* resources) {
    bool haveBounds = false;
    int minX = 0;
    int minY = 0;
    int maxX = 0;
    int maxY = 0;

    for (int i = 0; i < resources->noutput; ++i) {
	XRROutputInfo* info = XRRGetOutputInfo (this->m_display, resources, resources->outputs[i]);
	if (info == nullptr)
	    continue;

	if (info->connection != RR_Connected) {
	    XRRFreeOutputInfo (info);
	    continue;
	}

	XRRCrtcInfo* crtc = XRRGetCrtcInfo (this->m_display, resources, info->crtc);
	if (crtc == nullptr) {
	    XRRFreeOutputInfo (info);
	    continue;
	}

	bool requested = this->m_context.settings.general.screenBackgrounds.contains (info->name);
	if (!requested) {
	    for (const auto& spanGroup : this->m_context.settings.general.spanGroups) {
		if (std::ranges::find (spanGroup.screens, info->name) != spanGroup.screens.end ()) {
		    requested = true;
		    break;
		}
	    }
	}

	if (requested) {
	    sLog.out (
		"Found requested X11 output: ", info->name, " -> ", crtc->x, "x", crtc->y, ":", crtc->width,
		"x", crtc->height
	    );

	    auto* viewport = new GLFWOutputViewport { { crtc->x, crtc->y, crtc->width, crtc->height }, info->name };
	    viewport->globalPosition = { crtc->x, crtc->y };
	    viewport->logicalSize = { crtc->width, crtc->height };
	    this->m_screens.push_back (viewport);
	    this->m_viewports[info->name] = viewport;

	    const int right = crtc->x + static_cast<int> (crtc->width);
	    const int bottom = crtc->y + static_cast<int> (crtc->height);
	    if (!haveBounds) {
		minX = crtc->x;
		minY = crtc->y;
		maxX = right;
		maxY = bottom;
		haveBounds = true;
	    } else {
		minX = std::min (minX, crtc->x);
		minY = std::min (minY, crtc->y);
		maxX = std::max (maxX, right);
		maxY = std::max (maxY, bottom);
	    }
	}

	XRRFreeCrtcInfo (crtc);
	XRRFreeOutputInfo (info);
    }

    if (!haveBounds)
	return;

    this->m_rootOffsetX = minX;
    this->m_rootOffsetY = minY;
    this->m_fullWidth = maxX - minX;
    this->m_fullHeight = maxY - minY;

    // Render into a compact framebuffer whose origin is the first requested output.
    for (const auto& [name, viewport] : this->m_viewports) {
	viewport->viewport.x -= this->m_rootOffsetX;
	viewport->viewport.y -= this->m_rootOffsetY;
    }

    sLog.out (
	"X11 render bounds: ", this->m_fullWidth, "x", this->m_fullHeight, " @ ", this->m_rootOffsetX, "x",
	this->m_rootOffsetY, " (root ", this->m_rootWidth, "x", this->m_rootHeight, ")"
    );
}

void X11Output::validateOutputs () const {
    if (!this->m_viewports.empty ())
	return;

    sLog.error ("No requested X11 outputs could be initialized");
    sLog.error ("Requested outputs:");
    for (const auto& [name, background] : this->m_context.settings.general.screenBackgrounds)
	sLog.error ("  ", name);
    for (const auto& spanGroup : this->m_context.settings.general.spanGroups) {
	for (const auto& name : spanGroup.screens)
	    sLog.error ("  ", name, " (span)");
    }
    sLog.exception ("Cannot continue");
}

void X11Output::initX11Background () {
#ifdef HAVE_XFIXES
    int eventBase = 0;
    int errorBase = 0;
    int major = 0;
    int minor = 0;
    this->m_usePerOutputWindows = XFixesQueryExtension (this->m_display, &eventBase, &errorBase)
	&& XFixesQueryVersion (this->m_display, &major, &minor) && major >= 2;
#endif

    const int screen = DefaultScreen (this->m_display);
    const unsigned int depth = static_cast<unsigned int> (DefaultDepth (this->m_display, screen));

    if (this->m_usePerOutputWindows)
	this->initPerOutputWindows ();

    // Always mirror the live frame into the root pixmap. Compositors and
    // pseudo-transparent clients such as kitty consume these properties even
    // when the visible wallpaper itself is presented through desktop windows.
    this->m_pixmap = XCreatePixmap (
	this->m_display, this->m_root, static_cast<unsigned int> (this->m_rootWidth),
	static_cast<unsigned int> (this->m_rootHeight), depth
    );
    this->m_gc = XCreateGC (this->m_display, this->m_pixmap, 0, nullptr);

    XSetForeground (this->m_display, this->m_gc, BlackPixel (this->m_display, screen));
    XFillRectangle (
	this->m_display, this->m_pixmap, this->m_gc, 0, 0, static_cast<unsigned int> (this->m_rootWidth),
	static_cast<unsigned int> (this->m_rootHeight)
    );

    this->m_rootPixmapAtom = XInternAtom (this->m_display, "_XROOTPMAP_ID", False);
    this->m_esetrootPixmapAtom = XInternAtom (this->m_display, "ESETROOT_PMAP_ID", False);
    auto readPixmapProperty = [&] (Atom property) -> Pixmap {
	Atom actualType = None;
	int actualFormat = 0;
	unsigned long itemCount = 0;
	unsigned long bytesAfter = 0;
	unsigned char* data = nullptr;
	const int result = XGetWindowProperty (
	    this->m_display, this->m_root, property, 0, 1, False, XA_PIXMAP, &actualType, &actualFormat,
	    &itemCount, &bytesAfter, &data
	);
	Pixmap pixmap = None;
	if (result == Success && actualType == XA_PIXMAP && actualFormat == 32 && itemCount == 1 && data != nullptr)
	    pixmap = *reinterpret_cast<Pixmap*> (data);
	if (data != nullptr)
	    XFree (data);
	return pixmap;
    };

    Pixmap previousPixmap = readPixmapProperty (this->m_rootPixmapAtom);
    if (previousPixmap == None)
	previousPixmap = readPixmapProperty (this->m_esetrootPixmapAtom);

    if (previousPixmap != None) {
	Window pixmapRoot = None;
	int x = 0;
	int y = 0;
	unsigned int width = 0;
	unsigned int height = 0;
	unsigned int border = 0;
	unsigned int pixmapDepth = 0;
	if (XGetGeometry (
		this->m_display, previousPixmap, &pixmapRoot, &x, &y, &width, &height, &border, &pixmapDepth
	    )
	    && pixmapDepth == depth) {
	    XCopyArea (
		this->m_display, previousPixmap, this->m_pixmap, this->m_gc, 0, 0,
		std::min (width, static_cast<unsigned int> (this->m_rootWidth)),
		std::min (height, static_cast<unsigned int> (this->m_rootHeight)), 0, 0
	    );
	}
    }

    XSetWindowBackgroundPixmap (this->m_display, this->m_root, this->m_pixmap);
    XChangeProperty (
	this->m_display, this->m_root, this->m_rootPixmapAtom, XA_PIXMAP, 32, PropModeReplace,
	reinterpret_cast<unsigned char*> (&this->m_pixmap), 1
    );
    XChangeProperty (
	this->m_display, this->m_root, this->m_esetrootPixmapAtom, XA_PIXMAP, 32, PropModeReplace,
	reinterpret_cast<unsigned char*> (&this->m_pixmap), 1
    );

    this->initImageBuffer (depth);

    if (auto* driver = dynamic_cast<GLFWOpenGLDriver*> (&this->m_driver); driver != nullptr)
	driver->ensureX11RenderTargetSize ({ this->m_fullWidth, this->m_fullHeight });
    else
	sLog.exception ("X11 output requires the GLFW OpenGL driver");
}

void X11Output::initImageBuffer (unsigned int depth) {
    if (this->initShmImageBuffer (depth))
	return;

    this->initFallbackImageBuffer (depth);
}

bool X11Output::initShmImageBuffer (unsigned int depth) {
    if (!XShmQueryExtension (this->m_display))
	return false;

    const int screen = DefaultScreen (this->m_display);
    this->m_shmInfo = {};
    this->m_shmInfo.shmid = -1;
    this->m_image = XShmCreateImage (
	this->m_display, DefaultVisual (this->m_display, screen), depth, ZPixmap, nullptr, &this->m_shmInfo,
	this->m_fullWidth, this->m_fullHeight
    );
    if (this->m_image == nullptr)
	return false;

    if (!supportsBGRAReadback (this->m_image, this->m_fullWidth)) {
	sLog.error (
	    "MIT-SHM image layout is incompatible with BGRA readback (", this->m_image->bits_per_pixel,
	    " bits per pixel, ", this->m_image->bytes_per_line, " bytes per line, byte order ",
	    this->m_image->byte_order, ", RGB masks ", this->m_image->red_mask, "/", this->m_image->green_mask,
	    "/", this->m_image->blue_mask, ")"
	);
	this->discardShmImageBuffer ();
	return false;
    }

    const uint32_t imageBytes = checkedImageSize (this->m_image);
    this->m_shmInfo.shmid = shmget (IPC_PRIVATE, imageBytes, IPC_CREAT | 0600);
    if (this->m_shmInfo.shmid < 0) {
	this->discardShmImageBuffer ();
	return false;
    }

    this->m_shmInfo.shmaddr = static_cast<char*> (shmat (this->m_shmInfo.shmid, nullptr, 0));
    if (this->m_shmInfo.shmaddr == reinterpret_cast<char*> (-1)) {
	this->discardShmImageBuffer ();
	return false;
    }

    this->m_shmInfo.readOnly = False;
    this->m_image->data = this->m_shmInfo.shmaddr;
    trappedXError = false;
    trapXErrors = true;
    const bool attachRequested = XShmAttach (this->m_display, &this->m_shmInfo);
    XSync (this->m_display, False);
    trapXErrors = false;

    if (!attachRequested || trappedXError) {
	if (attachRequested)
	    sLog.error ("X server rejected MIT-SHM attachment; using regular X11 frame uploads");
	this->discardShmImageBuffer ();
	return false;
    }

    // Mark it for automatic removal after the final detach.
    shmctl (this->m_shmInfo.shmid, IPC_RMID, nullptr);
    this->m_useShm = true;
    this->m_imageData = this->m_shmInfo.shmaddr;
    this->m_imageSize = imageBytes;
    std::fill_n (this->m_imageData, imageBytes, 0);
    sLog.out ("Using MIT-SHM for X11 frame uploads (", imageBytes, " bytes)");
    return true;
}

void X11Output::discardShmImageBuffer () {
    if (this->m_shmInfo.shmaddr != nullptr && this->m_shmInfo.shmaddr != reinterpret_cast<char*> (-1))
	shmdt (this->m_shmInfo.shmaddr);
    if (this->m_shmInfo.shmid >= 0)
	shmctl (this->m_shmInfo.shmid, IPC_RMID, nullptr);
    if (this->m_image != nullptr) {
	this->m_image->data = nullptr;
	XDestroyImage (this->m_image);
	this->m_image = nullptr;
    }
    this->m_shmInfo = {};
}

void X11Output::initFallbackImageBuffer (unsigned int depth) {
    const int screen = DefaultScreen (this->m_display);
    this->m_image = XCreateImage (
	this->m_display, DefaultVisual (this->m_display, screen), depth, ZPixmap, 0, nullptr,
	this->m_fullWidth, this->m_fullHeight, 32, 0
    );
    if (this->m_image == nullptr)
	sLog.exception ("Cannot create the X11 image");
    if (!supportsBGRAReadback (this->m_image, this->m_fullWidth))
	sLog.exception (
	    "X11 image layout is incompatible with BGRA readback (", this->m_image->bits_per_pixel,
	    " bits per pixel, ", this->m_image->bytes_per_line, " bytes per line, byte order ",
	    this->m_image->byte_order, ", RGB masks ", this->m_image->red_mask, "/", this->m_image->green_mask,
	    "/", this->m_image->blue_mask, ")"
	);

    this->m_imageSize = checkedImageSize (this->m_image);
    this->m_imageData = static_cast<char*> (std::calloc (this->m_imageSize, 1));
    if (this->m_imageData == nullptr)
	sLog.exception ("Cannot allocate the X11 image buffer");
    this->m_image->data = this->m_imageData;

    sLog.out ("MIT-SHM unavailable; using regular X11 frame uploads");
}

void X11Output::initPerOutputWindows () {
#ifdef HAVE_XFIXES
    const Atom windowType = XInternAtom (this->m_display, "_NET_WM_WINDOW_TYPE", False);
    const Atom desktopType = XInternAtom (this->m_display, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
    const Atom windowState = XInternAtom (this->m_display, "_NET_WM_STATE", False);
    const Atom stateBelow = XInternAtom (this->m_display, "_NET_WM_STATE_BELOW", False);
    const Atom stateSticky = XInternAtom (this->m_display, "_NET_WM_STATE_STICKY", False);
    const Atom stateSkipTaskbar = XInternAtom (this->m_display, "_NET_WM_STATE_SKIP_TASKBAR", False);
    const Atom stateSkipPager = XInternAtom (this->m_display, "_NET_WM_STATE_SKIP_PAGER", False);
    const Atom windowDesktop = XInternAtom (this->m_display, "_NET_WM_DESKTOP", False);

    for (const auto& [name, viewport] : this->m_viewports) {
	XSetWindowAttributes attributes {};
	attributes.background_pixmap = None;

	const Window window = XCreateWindow (
	    this->m_display, this->m_root, viewport->globalPosition.x, viewport->globalPosition.y,
	    static_cast<unsigned int> (viewport->viewport.z), static_cast<unsigned int> (viewport->viewport.w), 0,
	    CopyFromParent, InputOutput, CopyFromParent, CWBackPixmap, &attributes
	);
	XChangeProperty (
	    this->m_display, window, windowType, XA_ATOM, 32, PropModeReplace,
	    reinterpret_cast<const unsigned char*> (&desktopType), 1
	);
	const Atom states[] = { stateBelow, stateSticky, stateSkipTaskbar, stateSkipPager };
	XChangeProperty (
	    this->m_display, window, windowState, XA_ATOM, 32, PropModeReplace,
	    reinterpret_cast<const unsigned char*> (states), 4
	);
	const unsigned long allDesktops = 0xFFFFFFFFUL;
	XChangeProperty (
	    this->m_display, window, windowDesktop, XA_CARDINAL, 32, PropModeReplace,
	    reinterpret_cast<const unsigned char*> (&allDesktops), 1
	);
	XStoreName (this->m_display, window, "linux-wallpaperengine desktop");

	const XserverRegion emptyRegion = XFixesCreateRegion (this->m_display, nullptr, 0);
	XFixesSetWindowShapeRegion (this->m_display, window, ShapeInput, 0, 0, emptyRegion);
	XFixesDestroyRegion (this->m_display, emptyRegion);

	XMapWindow (this->m_display, window);
	this->m_windows[name] = window;
	this->m_windowGCs[name] = XCreateGC (this->m_display, window, 0, nullptr);
    }

    XFlush (this->m_display);
#endif
}

void X11Output::updateRender () const {
    const auto putImage = [this] (
	Drawable drawable, GC gc, int sourceX, int sourceY, int destinationX, int destinationY, unsigned int width,
	unsigned int height
    ) {
	if (this->m_useShm) {
	    XShmPutImage (
		this->m_display, drawable, gc, this->m_image, sourceX, sourceY, destinationX, destinationY, width,
		height, False
	    );
	} else {
	    XPutImage (
		this->m_display, drawable, gc, this->m_image, sourceX, sourceY, destinationX, destinationY, width,
		height
	    );
	}
    };

    if (this->m_usePerOutputWindows) {
	for (const auto& [name, viewport] : this->m_viewports) {
	    const auto window = this->m_windows.find (name);
	    const auto gc = this->m_windowGCs.find (name);
	    if (window == this->m_windows.end () || gc == this->m_windowGCs.end ())
		continue;

	    putImage (
		window->second, gc->second, viewport->viewport.x, viewport->viewport.y, 0, 0,
		static_cast<unsigned int> (viewport->viewport.z), static_cast<unsigned int> (viewport->viewport.w)
	    );
	}
    }

    const auto now = std::chrono::steady_clock::now ();
    const bool syncRoot = !this->m_usePerOutputWindows || this->m_lastRootSync.time_since_epoch ().count () == 0
	|| now - this->m_lastRootSync >= std::chrono::seconds (1);
    if (syncRoot) {
	// Keep pseudo-transparency and compositor blur compatible, but avoid
	// uploading an additional full desktop image on every rendered frame.
	for (const auto& [name, viewport] : this->m_viewports) {
	    putImage (
		this->m_pixmap, this->m_gc, viewport->viewport.x, viewport->viewport.y, viewport->globalPosition.x,
		viewport->globalPosition.y, static_cast<unsigned int> (viewport->viewport.z),
		static_cast<unsigned int> (viewport->viewport.w)
	    );
	    XClearArea (
		this->m_display, this->m_root, viewport->globalPosition.x, viewport->globalPosition.y,
		static_cast<unsigned int> (viewport->viewport.z), static_cast<unsigned int> (viewport->viewport.w),
		False
	    );
	}

	Pixmap pixmap = this->m_pixmap;
	XChangeProperty (
	    this->m_display, this->m_root, this->m_rootPixmapAtom, XA_PIXMAP, 32, PropModeReplace,
	    reinterpret_cast<unsigned char*> (&pixmap), 1
	);
	XChangeProperty (
	    this->m_display, this->m_root, this->m_esetrootPixmapAtom, XA_PIXMAP, 32, PropModeReplace,
	    reinterpret_cast<unsigned char*> (&pixmap), 1
	);
	this->m_lastRootSync = now;
    }

    // The renderer overwrites this buffer on the next frame, so ensure the X
    // server has finished consuming shared-memory uploads before returning.
    if (this->m_useShm)
	XSync (this->m_display, False);
    else
	XFlush (this->m_display);
}
