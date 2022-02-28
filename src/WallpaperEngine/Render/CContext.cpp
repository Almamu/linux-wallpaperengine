#include <iostream>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>

#include <GL/glew.h>
#include <GL/glx.h>

#include "CContext.h"
#include "CVideo.h"

using namespace WallpaperEngine::Render;

CContext::CContext (std::vector <std::string> screens) :
    m_wallpaper (nullptr),
    m_screens (std::move (screens)),
    m_isRootWindow (m_screens.empty () == false),
    m_defaultViewport ({0, 0, 1920, 1080})
{
    this->initializeViewports ();
}

void CContext::initializeViewports ()
{
    if (this->m_isRootWindow == false || this->m_screens.empty () == true)
        return;

    this->m_display = XOpenDisplay (nullptr);

    int xrandr_result, xrandr_error;

    if (!XRRQueryExtension (this->m_display, &xrandr_result, &xrandr_error))
    {
        std::cerr << "XRandr is not present, cannot detect specified screens, running in window mode" << std::endl;
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
        if (info == nullptr)
            continue;

        auto cur = this->m_screens.begin ();
        auto end = this->m_screens.end ();

        for (; cur != end; cur ++)
        {
            if (info->connection == RR_Connected && strcmp (info->name, (*cur).c_str ()) == 0)
            {
                XRRCrtcInfo* crtc = XRRGetCrtcInfo (this->m_display, screenResources, info->crtc);

                std::cout << "Found requested screen: " << info->name << " -> " << crtc->x << "x" << crtc->y << ":" << crtc->width << "x" << crtc->height << std::endl;

                glm::ivec4 viewport = {
                    crtc->x, fullHeight - (crtc->y + crtc->height), crtc->width, crtc->height
                };

                this->m_viewports.push_back (viewport);

                XRRFreeCrtcInfo (crtc);
            }
        }

        XRRFreeOutputInfo (info);
    }

    XRRFreeScreenResources (screenResources);

    // create the fbo that will handle the screen
    this->m_fbo = new CFBO("_sc_FullFrameBuffer", ITexture::TextureFormat::ARGB8888, 1.0, fullWidth, fullHeight, fullWidth, fullHeight);

    // set the window background so the pixmap is drawn
    XSetWindowBackgroundPixmap(this->m_display, root, this->m_pixmap);

    this->m_imageData = new char [fullWidth * fullHeight * 4];

    // create the image for X11 to be able to copy it over
    this->m_image = XCreateImage (this->m_display, CopyFromParent, 24, ZPixmap, 0, this->m_imageData, fullWidth, fullHeight, 32, 0);

    // Cause of issue for issue #59 origial issue
    // glfwWindowHintPointer (GLFW_NATIVE_PARENT_HANDLE, reinterpret_cast <void*> (DefaultRootWindow (display)));
}

void CContext::render ()
{
    if (this->m_wallpaper == nullptr)
        return;

    if (this->m_viewports.empty () == false)
    {
        bool firstFrame = true;
        bool renderFrame = true;
        auto cur = this->m_viewports.begin ();
        auto end = this->m_viewports.end ();
        int fullWidth = DisplayWidth (this->m_display, DefaultScreen (this->m_display));
        int fullHeight = DisplayHeight (this->m_display, DefaultScreen (this->m_display));
        Window root = DefaultRootWindow (this->m_display);

        for (; cur != end; cur ++)
        {
            // render the background
            this->m_wallpaper->render (*cur, renderFrame, firstFrame);
            // scenes need to render a new frame for each viewport as they produce different results
            // but videos should only be rendered once per group of viewports
            firstFrame = false;
            renderFrame = !this->m_wallpaper->is <CVideo> ();
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
    else
        this->m_wallpaper->render (this->m_defaultViewport);
}

void CContext::setWallpaper (CWallpaper* wallpaper)
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

void CContext::setDefaultViewport (glm::vec4 defaultViewport)
{
    this->m_defaultViewport = defaultViewport;
}

CMouseInput* CContext::getMouse () const
{
    return this->m_mouse;
}

void CContext::setMouse (CMouseInput* mouse)
{
    this->m_mouse = mouse;
}