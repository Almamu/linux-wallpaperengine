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

    m_display = XOpenDisplay (nullptr);

    int xrandr_result, xrandr_error;

    if (!XRRQueryExtension (m_display, &xrandr_result, &xrandr_error))
    {
        std::cerr << "XRandr is not present, cannot detect specified screens, running in window mode" << std::endl;
        return;
    }

    int fullWidth = DisplayWidth (m_display, DefaultScreen (m_display));
    int fullHeight = DisplayHeight (m_display, DefaultScreen (m_display));
    XRRScreenResources* screenResources = XRRGetScreenResources (m_display, DefaultRootWindow (m_display));

    // there are some situations where xrandr returns null (like screen not using the extension)
    if (screenResources == nullptr)
        return;

    for (int i = 0; i < screenResources->noutput; i ++)
    {
        XRROutputInfo* info = XRRGetOutputInfo (m_display, screenResources, screenResources->outputs [i]);

        // there are some situations where xrandr returns null (like screen not using the extension)
        if (info == nullptr)
            continue;

        auto cur = this->m_screens.begin ();
        auto end = this->m_screens.end ();

        for (; cur != end; cur ++)
        {
            if (info->connection == RR_Connected && strcmp (info->name, (*cur).c_str ()) == 0)
            {
                XRRCrtcInfo* crtc = XRRGetCrtcInfo (m_display, screenResources, info->crtc);

                std::cout << "Found requested screen: " << info->name << " -> " << crtc->x << "x" << crtc->y << ":" << crtc->width << "x" << crtc->height << std::endl;

                glm::vec4 viewport = {
                    crtc->x, fullHeight - (crtc->y + crtc->height), crtc->width, crtc->height
                };

                this->m_viewports.push_back (viewport);

                XRRFreeCrtcInfo (crtc);
            }
        }

        XRRFreeOutputInfo (info);
    }

    XRRFreeScreenResources (screenResources);

    // Cause of issue for issue #59 origial issue
    // glfwWindowHintPointer (GLFW_NATIVE_PARENT_HANDLE, reinterpret_cast <void*> (DefaultRootWindow (m_display)));
}

void CContext::render ()
{
    if (this->m_wallpaper == nullptr)
        return;

    if (this->m_viewports.empty () == false)
    {
        auto cur = this->m_viewports.begin ();
        auto end = this->m_viewports.end ();

        Window root = DefaultRootWindow(m_display);
        int windowWidth = 1920, windowHeight = 1080;
        int fullWidth = DisplayWidth (m_display, DefaultScreen (m_display));
        int fullHeight = DisplayHeight (m_display, DefaultScreen (m_display));

        m_pm = XCreatePixmap(m_display, root, fullWidth, fullHeight, 24);
        m_gc = XCreateGC(m_display, m_pm, 0, NULL);
        XFillRectangle(m_display, m_pm, m_gc, 0, 0, fullWidth, fullHeight);

        char* image_data;
        image_data = new char[windowWidth*windowHeight*4];

        this->m_wallpaper->render (this->m_defaultViewport, true, image_data);
        XImage* image = XCreateImage(m_display, CopyFromParent, 24, ZPixmap, 0, (char *)image_data, windowWidth, windowHeight, 32, 0);
        for (; cur != end; cur ++)
        {
            XPutImage(m_display, m_pm, m_gc, image, 0, 0, (*cur).x, (*cur).y, windowWidth, windowHeight);
        }

        // _XROOTPMAP_ID & ESETROOT_PMAP_ID allow other programs (compositors) to 
        // edit the background. Without these, other programs will clear the screen.
        Atom prop_root = XInternAtom(m_display, "_XROOTPMAP_ID", False);
        Atom prop_esetroot = XInternAtom(m_display, "ESETROOT_PMAP_ID", False);
        XChangeProperty(m_display, root, prop_root, XA_PIXMAP, 32, PropModeReplace, (unsigned char *) &m_pm, 1);
        XChangeProperty(m_display, root, prop_esetroot, XA_PIXMAP, 32, PropModeReplace, (unsigned char *) &m_pm, 1);

        XSetWindowBackgroundPixmap(m_display, root, m_pm);
        XClearWindow(m_display, root);
        XFlush(m_display);

        XDestroyImage(image);
    }
    else
        this->m_wallpaper->render (this->m_defaultViewport);
}

void CContext::setWallpaper (CWallpaper* wallpaper)
{
    this->m_wallpaper = wallpaper;
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