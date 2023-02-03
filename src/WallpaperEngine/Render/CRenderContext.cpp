#include "common.h"
#include <iostream>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>

#include <GL/glew.h>
#include <GL/glx.h>

#include "CRenderContext.h"
#include "CVideo.h"

#define DEFAULT_WINDOW_WIDTH 1280
#define DEFAULT_WINDOW_HEIGHT 720

using namespace WallpaperEngine::Render;

XErrorHandler originalErrorHandler;

void CustomXIOErrorExitHandler (Display* dsp, void* userdata)
{
    auto context = static_cast <CRenderContext*> (userdata);

#ifdef DEBUG
    sLog.debugerror ("Critical XServer error detected. Attempting to recover...");
#endif /* DEBUG */

    // refetch all the resources
    context->initialize ();
}

int CustomXErrorHandler (Display* dpy, XErrorEvent* event)
{
#ifdef DEBUG
    sLog.debugerror ("Detected X error");
#endif /* DEBUG */

    // call the original handler so we can keep some information reporting
    originalErrorHandler (dpy, event);

    return 0;
}

int CustomXIOErrorHandler (Display* dsp)
{
#ifdef DEBUG
    sLog.debugerror ("Detected X error");
#endif /* DEBUG */

    return 0;
}

CRenderContext::CRenderContext (std::vector <std::string> screens, CVideoDriver& driver, CContainer* container) :
    m_wallpaper (nullptr),
    m_screens (std::move (screens)),
    m_driver (driver),
    m_container (container),
    m_textureCache (new CTextureCache (this))
{
    this->initialize ();
}

void CRenderContext::initialize ()
{
    if (this->m_screens.empty () == true)
        this->setupWindow ();
    else
        this->setupScreens ();
}

void CRenderContext::setupWindow ()
{
    this->m_driver.showWindow ();
    this->m_driver.resizeWindow ({DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT});
}

void CRenderContext::setupScreens ()
{
    this->m_viewports.clear ();

    this->m_display = XOpenDisplay (nullptr);

    // set the error handling to try and recover from X disconnections
#ifdef HAVE_XSETIOERROREXITHANDLER
    XSetIOErrorExitHandler (this->m_display, CustomXIOErrorExitHandler, this);
#endif /* HAVE_XSETIOERROREXITHANDLER */
    originalErrorHandler = XSetErrorHandler (CustomXErrorHandler);
    XSetIOErrorHandler (CustomXIOErrorHandler);

    int xrandr_result, xrandr_error;

    if (!XRRQueryExtension (this->m_display, &xrandr_result, &xrandr_error))
    {
        sLog.error ("XRandr is not present, cannot detect specified screens, running in window mode");
        return;
    }

    Window root = DefaultRootWindow (this->m_display);
    int fullWidth = DisplayWidth (this->m_display, DefaultScreen (this->m_display));
    int fullHeight = DisplayHeight (this->m_display, DefaultScreen (this->m_display));
    XRRScreenResources* screenResources = XRRGetScreenResources (this->m_display, DefaultRootWindow (this->m_display));

    // there are some situations where xrandr returns null (like screen not using the extension)
    if (screenResources == nullptr)
        return;

    // create the pixmap and gc used for this display, most situations only have one "display"
    this->m_pixmap = XCreatePixmap (this->m_display, DefaultRootWindow (this->m_display), fullWidth, fullHeight, 24);
    this->m_gc = XCreateGC (this->m_display, this->m_pixmap, 0, nullptr);
    // fill the whole pixmap with black for now
    XFillRectangle (this->m_display, this->m_pixmap, this->m_gc, 0, 0, fullWidth, fullHeight);

    for (int i = 0; i < screenResources->noutput; i ++)
    {
        XRROutputInfo* info = XRRGetOutputInfo (this->m_display, screenResources, screenResources->outputs [i]);

        // there are some situations where xrandr returns null (like screen not using the extension)
        if (info == nullptr || info->connection != RR_Connected)
            continue;

        for (const auto& cur : this->m_screens)
        {
            if (strcmp (info->name, cur.c_str ()) != 0)
                continue;

            XRRCrtcInfo* crtc = XRRGetCrtcInfo (this->m_display, screenResources, info->crtc);

            sLog.out ("Found requested screen: ", info->name, " -> ", crtc->x, "x", crtc->y, ":", crtc->width, "x", crtc->height);

            this->m_viewports.push_back ({{crtc->x, crtc->y, crtc->width, crtc->height}, cur});

            XRRFreeCrtcInfo (crtc);
        }

        XRRFreeOutputInfo (info);
    }

    XRRFreeScreenResources (screenResources);

    // create the fbo that will handle the screen
    this->m_fbo = new CFBO(
        "_sc_FullFrameBuffer",
        ITexture::TextureFormat::ARGB8888,
        ITexture::TextureFlags::NoFlags,
        1.0,
        fullWidth, fullHeight,
        fullWidth, fullHeight
    );

    // set the window background so the pixmap is drawn
    XSetWindowBackgroundPixmap(this->m_display, root, this->m_pixmap);

    this->m_imageData = new char [fullWidth * fullHeight * 4];

    // create the image for X11 to be able to copy it over
    this->m_image = XCreateImage (this->m_display, CopyFromParent, 24, ZPixmap, 0, this->m_imageData, fullWidth, fullHeight, 32, 0);
}

CRenderContext::~CRenderContext ()
{
    if (this->m_screens.empty () == false)
    {
        // free any used resource
        XDestroyImage (this->m_image);
        XFreeGC (this->m_display, this->m_gc);
        XFreePixmap (this->m_display, this->m_pixmap);
        XCloseDisplay (this->m_display);
    }
}

void CRenderContext::renderScreens ()
{
    bool firstFrame = true;
    bool renderFrame = true;
    int fullWidth = DisplayWidth (this->m_display, DefaultScreen (this->m_display));
    int fullHeight = DisplayHeight (this->m_display, DefaultScreen (this->m_display));
    Window root = DefaultRootWindow (this->m_display);

    for (const auto& cur : this->m_viewports)
    {
        if (DEBUG)
        {
            std::string str = "Rendering to screen " + cur.name;

            glPushDebugGroup (GL_DEBUG_SOURCE_APPLICATION, 0, -1, str.c_str ());
        }

        // render the background
        this->m_wallpaper->render (cur.viewport, false, renderFrame, firstFrame);
        // scenes need to render a new frame for each viewport as they produce different results
        // but videos should only be rendered once per group of viewports
        firstFrame = false;
        renderFrame = !this->m_wallpaper->is <CVideo> ();

        if (DEBUG)
            glPopDebugGroup ();
    }

    // read the full texture into the image
    glReadPixels (0, 0, fullWidth, fullHeight, GL_BGRA, GL_UNSIGNED_BYTE, (void*) this->m_imageData);

    // put the image back into the screen
    XPutImage (this->m_display, this->m_pixmap, this->m_gc, this->m_image, 0, 0, 0, 0, fullWidth, fullHeight);

    // _XROOTPMAP_ID & ESETROOT_PMAP_ID allow other programs (compositors) to
    // edit the background. Without these, other programs will clear the screen.
    // it also forces the compositor to refresh the background (tested with picom)
    Atom prop_root = XInternAtom(this->m_display, "_XROOTPMAP_ID", False);
    Atom prop_esetroot = XInternAtom(this->m_display, "ESETROOT_PMAP_ID", False);
    XChangeProperty(this->m_display, root, prop_root, XA_PIXMAP, 32, PropModeReplace, (unsigned char *) &this->m_pixmap, 1);
    XChangeProperty(this->m_display, root, prop_esetroot, XA_PIXMAP, 32, PropModeReplace, (unsigned char *) &this->m_pixmap, 1);

    XClearWindow(this->m_display, root);
    XFlush(this->m_display);
}

void CRenderContext::renderWindow ()
{
    // render the background to the window directly
    this->m_wallpaper->render ({0, 0, this->m_driver.getFramebufferSize ()}, true);
}

void CRenderContext::render ()
{
    if (this->m_wallpaper == nullptr)
        return;

    // ensure mouse information is up to date before drawing any frame
    this->m_mouse->update ();

    if (this->m_viewports.empty () == false)
        this->renderScreens ();
    else
        this->renderWindow ();

    this->m_driver.swapBuffers ();
}

void CRenderContext::setWallpaper (CWallpaper* wallpaper)
{
    this->m_wallpaper = wallpaper;

    // update the wallpaper's texcoords based on the mode we're running
    if (this->m_screens.empty () == false)
    {
        GLfloat texCoords [] = {
            0.0f, 1.0f,
            1.0f, 1.0f,
            0.0f, 0.0f,
            0.0f, 0.0f,
            1.0f, 1.0f,
            1.0f, 0.0f
        };

        this->m_wallpaper->updateTexCoord (texCoords, sizeof (texCoords));
        this->m_wallpaper->setDestinationFramebuffer (this->m_fbo->getFramebuffer ());
    }
}

CMouseInput* CRenderContext::getMouse () const
{
    return this->m_mouse;
}

void CRenderContext::setMouse (CMouseInput* mouse)
{
    this->m_mouse = mouse;
}

CWallpaper* CRenderContext::getWallpaper () const
{
    return this->m_wallpaper;
}

const CContainer* CRenderContext::getContainer () const
{
    return this->m_container;
}

const ITexture* CRenderContext::resolveTexture (const std::string& name)
{
    return this->m_textureCache->resolve (name);
}