#include "common.h"
#include "CX11Output.h"
#include "CX11OutputViewport.h"

#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>

#define FULLSCREEN_CHECK_WAIT_TIME 250

using namespace WallpaperEngine::Render::Drivers::Output;

void CustomXIOErrorExitHandler (Display* dsp, void* userdata)
{
    auto context = static_cast <CX11Output*> (userdata);

    sLog.debugerror ("Critical XServer error detected. Attempting to recover...");

    // refetch all the resources
    context->reset ();
}

int CustomXErrorHandler (Display* dpy, XErrorEvent* event)
{
    sLog.debugerror ("Detected X error");

    return 0;
}

int CustomXIOErrorHandler (Display* dsp)
{
    sLog.debugerror ("Detected X error");

    return 0;
}

CX11Output::CX11Output (CApplicationContext& context, CVideoDriver& driver) :
    COutput (context, driver)
{
    // do not use previous handler, it might stop the app under weird circumstances
    XSetErrorHandler (CustomXErrorHandler);
    XSetIOErrorHandler (CustomXIOErrorHandler);

    this->loadScreenInfo ();
}

CX11Output::~CX11Output ()
{
    this->free ();
}

void CX11Output::reset ()
{
    // first free whatever we have right now
    this->free ();
    // re-load screen info
    this->loadScreenInfo ();
    // do the same for the detector
    this->m_driver.getFullscreenDetector ().reset ();
}

void CX11Output::free ()
{
    // go through all the viewports and free them
    for(const auto& cur : this->m_viewports)
        delete cur.second;

    this->m_viewports.clear ();

    // free all the resources we've got
    XDestroyImage (this->m_image);
    XFreeGC (this->m_display, this->m_gc);
    XFreePixmap (this->m_display, this->m_pixmap);
    delete this->m_imageData;
    XCloseDisplay (this->m_display);
}

void* CX11Output::getImageBuffer () const
{
    return this->m_imageData;
}

bool CX11Output::renderVFlip () const
{
    return false;
}

bool CX11Output::renderMultiple () const
{
    return this->m_viewports.size () > 1;
}

bool CX11Output::haveImageBuffer () const
{
    return true;
}

void CX11Output::loadScreenInfo ()
{
    // reset the viewports
    this->m_viewports.clear ();

    this->m_display = XOpenDisplay (nullptr);
    // set the error handling to try and recover from X disconnections
#ifdef HAVE_XSETIOERROREXITHANDLER
    XSetIOErrorExitHandler (this->m_display, CustomXIOErrorExitHandler, this);
#endif /* HAVE_XSETIOERROREXITHANDLER */

    int xrandr_result, xrandr_error;

    if (!XRRQueryExtension (this->m_display, &xrandr_result, &xrandr_error))
    {
        sLog.error ("XRandr is not present, cannot detect specified screens, running in window mode");
        return;
    }

    this->m_root = DefaultRootWindow (this->m_display);
    this->m_fullWidth = DisplayWidth (this->m_display, DefaultScreen (this->m_display));
    this->m_fullHeight = DisplayHeight (this->m_display, DefaultScreen (this->m_display));
    XRRScreenResources* screenResources = XRRGetScreenResources (this->m_display, DefaultRootWindow (this->m_display));

    if (screenResources == nullptr)
    {
        sLog.error ("Cannot detect screen sizes using xrandr, running in window mode");
        return;
    }

    for (int i = 0; i < screenResources->noutput; i ++)
    {
        XRROutputInfo* info = XRRGetOutputInfo (this->m_display, screenResources, screenResources->outputs [i]);

        // screen not in use, ignore it
        if (info == nullptr || info->connection != RR_Connected)
            continue;

        XRRCrtcInfo* crtc = XRRGetCrtcInfo (this->m_display, screenResources, info->crtc);

	// screen not active, ignore it
	if (crtc == nullptr)
		continue;

        // add the screen to the list of screens
        this->m_screens.push_back (
            new CX11OutputViewport
            {
                {crtc->x, crtc->y, crtc->width, crtc->height},
                info->name
            }
        );

        // only keep info of registered screens
        if (this->m_context.settings.general.screenBackgrounds.find (info->name) != this->m_context.settings.general.screenBackgrounds.end ())
        {
            sLog.out ("Found requested screen: ", info->name, " -> ", crtc->x, "x", crtc->y, ":", crtc->width, "x", crtc->height);

            this->m_viewports[info->name] =
                new CX11OutputViewport
                {
                    {crtc->x, crtc->y, crtc->width, crtc->height},
                    info->name
                };
        }

        XRRFreeCrtcInfo (crtc);
    }

    XRRFreeScreenResources (screenResources);

    // Check if all screens from --screen-root are actually screens
    if (this->m_viewports.size() != this->m_context.settings.general.screenBackgrounds.size()) {
	    sLog.exception("Invalid screen in arguments.");
    }

    // create pixmap so we can draw things in there
    this->m_pixmap = XCreatePixmap (this->m_display, this->m_root, this->m_fullWidth, this->m_fullHeight, 24);
    this->m_gc = XCreateGC (this->m_display, this->m_pixmap, 0, nullptr);
    // pre-fill it with black
    XFillRectangle (this->m_display, this->m_pixmap, this->m_gc, 0, 0, this->m_fullWidth, this->m_fullHeight);
    // set the window background as our pixmap
    XSetWindowBackgroundPixmap (this->m_display, this->m_root, this->m_pixmap);
    // allocate space for the image's data
    this->m_imageData = new char [this->m_fullWidth * this->m_fullHeight * 4];
    // create an image so we can copy it over
    this->m_image = XCreateImage (this->m_display, CopyFromParent, 24, ZPixmap, 0, this->m_imageData, this->m_fullWidth, this->m_fullHeight, 32, 0);
    // setup driver's render changing the window's size
    this->m_driver.resizeWindow ({this->m_fullWidth, this->m_fullHeight});
}

void CX11Output::updateRender () const
{
    // put the image back into the screen
    XPutImage (this->m_display, this->m_pixmap, this->m_gc, this->m_image, 0, 0, 0, 0, this->m_fullWidth, this->m_fullHeight);

    // _XROOTPMAP_ID & ESETROOT_PMAP_ID allow other programs (compositors) to
    // edit the background. Without these, other programs will clear the screen.
    // it also forces the compositor to refresh the background (tested with picom)
    Atom prop_root = XInternAtom(this->m_display, "_XROOTPMAP_ID", False);
    Atom prop_esetroot = XInternAtom(this->m_display, "ESETROOT_PMAP_ID", False);
    XChangeProperty(this->m_display, this->m_root, prop_root, XA_PIXMAP, 32, PropModeReplace, (unsigned char *) &this->m_pixmap, 1);
    XChangeProperty(this->m_display, this->m_root, prop_esetroot, XA_PIXMAP, 32, PropModeReplace, (unsigned char *) &this->m_pixmap, 1);

    XClearWindow(this->m_display, this->m_root);
    XFlush(this->m_display);

    // check for fullscreen windows and wait until there's none fullscreen
    while (this->m_driver.getFullscreenDetector ().anythingFullscreen () && this->m_context.state.general.keepRunning)
        usleep (FULLSCREEN_CHECK_WAIT_TIME);
}
