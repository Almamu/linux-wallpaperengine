#include "GLFWOutputViewport.h"
#include "WallpaperEngine/Logging/Log.h"
#include "X11Output.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

using namespace WallpaperEngine::Render::Drivers::Output;

void CustomXIOErrorExitHandler (Display* dsp, void* userdata) {
    const auto context = static_cast<X11Output*> (userdata);

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

X11Output::X11Output (ApplicationContext& context, VideoDriver& driver) : Output (context, driver),
    m_display (nullptr),
    m_pixmap (None),
    m_root (None),
    m_gc (None),
    m_imageData (nullptr),
    m_imageSize (0),
    m_image (nullptr) {
    // do not use previous handler, it might stop the app under weird circumstances
    XSetErrorHandler (CustomXErrorHandler);
    XSetIOErrorHandler (CustomXIOErrorHandler);

    this->loadScreenInfo ();
}

X11Output::~X11Output () {
    this->free ();
}

void X11Output::reset () {
    // first free whatever we have right now
    this->free ();
    // re-load screen info
    this->loadScreenInfo ();
    // do the same for the detector
    // TODO: BRING BACK THIS FUNCTIONALITY
    // this->m_driver.getFullscreenDetector ().reset ();
}

void X11Output::free () {
    // go through all the viewports and free them
    for (const auto& [screen, viewport] : this->m_viewports)
        delete viewport;

    this->m_viewports.clear ();

    // free all the resources we've got
    XDestroyImage (this->m_image);
    XFreeGC (this->m_display, this->m_gc);
    XFreePixmap (this->m_display, this->m_pixmap);
    delete this->m_imageData;
    XCloseDisplay (this->m_display);
}

void* X11Output::getImageBuffer () const {
    return this->m_imageData;
}

bool X11Output::renderVFlip () const {
    return false;
}

bool X11Output::renderMultiple () const {
    return this->m_viewports.size () > 1;
}

bool X11Output::haveImageBuffer () const {
    return true;
}

uint32_t X11Output::getImageBufferSize () const {
    return this->m_imageSize;
}

void X11Output::loadScreenInfo () {
    this->m_display = XOpenDisplay (nullptr);
    // set the error handling to try and recover from X disconnections
#ifdef HAVE_XSETIOERROREXITHANDLER
    XSetIOErrorExitHandler (this->m_display, CustomXIOErrorExitHandler, this);
#endif /* HAVE_XSETIOERROREXITHANDLER */

    int xrandr_result, xrandr_error;

    if (!XRRQueryExtension (this->m_display, &xrandr_result, &xrandr_error)) {
        sLog.error ("XRandr is not present, cannot detect specified screens, running in window mode");
        return;
    }

    this->m_root = DefaultRootWindow (this->m_display);
    this->m_fullWidth = DisplayWidth (this->m_display, DefaultScreen (this->m_display));
    this->m_fullHeight = DisplayHeight (this->m_display, DefaultScreen (this->m_display));
    XRRScreenResources* screenResources = XRRGetScreenResources (this->m_display, DefaultRootWindow (this->m_display));

    if (screenResources == nullptr) {
        sLog.error ("Cannot detect screen sizes using xrandr, running in window mode");
        return;
    }

    for (int i = 0; i < screenResources->noutput; i++) {
        const XRROutputInfo* info = XRRGetOutputInfo (this->m_display, screenResources, screenResources->outputs [i]);

        // screen not in use, ignore it
        if (info == nullptr || info->connection != RR_Connected)
            continue;

        XRRCrtcInfo* crtc = XRRGetCrtcInfo (this->m_display, screenResources, info->crtc);

        // screen not active, ignore it
        if (crtc == nullptr)
            continue;

        // add the screen to the list of screens
        this->m_screens.push_back (new GLFWOutputViewport {{crtc->x, crtc->y, crtc->width, crtc->height}, info->name});

        // only keep info of registered screens
        if (this->m_context.settings.general.screenBackgrounds.find (info->name) !=
            this->m_context.settings.general.screenBackgrounds.end ()) {
            sLog.out ("Found requested screen: ", info->name, " -> ", crtc->x, "x", crtc->y, ":", crtc->width, "x",
                      crtc->height);

            this->m_viewports [info->name] =
                new GLFWOutputViewport {{crtc->x, crtc->y, crtc->width, crtc->height}, info->name};
        }

        XRRFreeCrtcInfo (crtc);
    }

    XRRFreeScreenResources (screenResources);

    bool any = false;

    for (const auto& o : this->m_screens) {
        const auto cur = this->m_context.settings.general.screenBackgrounds.find (o->name);

        if (cur == this->m_context.settings.general.screenBackgrounds.end ())
            continue;

        any = true;
    }

    if (!any)
        sLog.exception ("No outputs could be initialized, please check the parameters and try again");

    // create pixmap so we can draw things in there
    this->m_pixmap = XCreatePixmap (this->m_display, this->m_root, this->m_fullWidth, this->m_fullHeight, 24);
    this->m_gc = XCreateGC (this->m_display, this->m_pixmap, 0, nullptr);
    // pre-fill it with black
    XFillRectangle (this->m_display, this->m_pixmap, this->m_gc, 0, 0, this->m_fullWidth, this->m_fullHeight);
    // set the window background as our pixmap
    XSetWindowBackgroundPixmap (this->m_display, this->m_root, this->m_pixmap);
    // allocate space for the image's data
    this->m_imageSize = this->m_fullWidth * this->m_fullHeight * 4;
    this->m_imageData = new char [this->m_fullWidth * this->m_fullHeight * 4];
    // create an image so we can copy it over
    this->m_image = XCreateImage (this->m_display, CopyFromParent, 24, ZPixmap, 0, this->m_imageData, this->m_fullWidth,
                                  this->m_fullHeight, 32, 0);
    // setup driver's render changing the window's size
    this->m_driver.resizeWindow ({this->m_fullWidth, this->m_fullHeight});
}

void X11Output::updateRender () const {
    // put the image back into the screen
    XPutImage (this->m_display, this->m_pixmap, this->m_gc, this->m_image, 0, 0, 0, 0, this->m_fullWidth,
               this->m_fullHeight);

    // _XROOTPMAP_ID & ESETROOT_PMAP_ID allow other programs (compositors) to
    // edit the background. Without these, other programs will clear the screen.
    // it also forces the compositor to refresh the background (tested with picom)
    const Atom prop_root = XInternAtom (this->m_display, "_XROOTPMAP_ID", False);
    const Atom prop_esetroot = XInternAtom (this->m_display, "ESETROOT_PMAP_ID", False);
    XChangeProperty (this->m_display, this->m_root, prop_root, XA_PIXMAP, 32, PropModeReplace,
                     (unsigned char*) &this->m_pixmap, 1);
    XChangeProperty (this->m_display, this->m_root, prop_esetroot, XA_PIXMAP, 32, PropModeReplace,
                     (unsigned char*) &this->m_pixmap, 1);

    XClearWindow (this->m_display, this->m_root);
    XFlush (this->m_display);
}
