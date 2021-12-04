#include "CWindow.h"

using namespace WallpaperEngine::Core;

void CWindow::MakeWindowsDesktop(GLFWwindow *window)
{

    Window xwindow = glfwGetX11Window(window);
    Display *xdisplay = glfwGetX11Display();

    Atom xa = XInternAtom(xdisplay, "_NET_WM_WINDOW_TYPE", False);
    Atom type = XInternAtom(xdisplay, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
    XChangeProperty(xdisplay, xwindow,
                    xa, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)&type, 1);

    xa = XInternAtom(xdisplay, "_NET_WM_DESKTOP", False);
    if (xa != None)
    {
        unsigned int xa_prop = 0xFFFFFFFF;
        XChangeProperty(xdisplay, xwindow, xa, XA_CARDINAL, 32,
                        PropModeAppend,
                        (unsigned char *)&xa_prop, 1);
    }

    xa = XInternAtom(xdisplay, "_NET_WM_STATE", False);
    if (xa != None)
    {
        Atom xa_prop = XInternAtom(xdisplay, "_NET_WM_STATE_STICKY", False);
        XChangeProperty(xdisplay, xwindow, xa, XA_ATOM, 32,
                        PropModeAppend,
                        (unsigned char *)&xa_prop, 1);
    }

    xa = XInternAtom(xdisplay, "_WIN_LAYER", False);
    if (xa != None)
    {
        long prop = 0;

        XChangeProperty(xdisplay, xwindow, xa, XA_CARDINAL, 32,
                        PropModeAppend,
                        (unsigned char *)&prop, 1);
    }

    xa = XInternAtom(xdisplay, "_NET_WM_STATE", False);
    if (xa != None)
    {
        Atom xa_prop = XInternAtom(xdisplay, "_NET_WM_STATE_BELOW", False);

        XChangeProperty(xdisplay, xwindow, xa, XA_ATOM, 32,
                        PropModeAppend,
                        (unsigned char *)&xa_prop, 1);
    }

    xa = XInternAtom(xdisplay, "_NET_WM_STATE", False);
    if (xa != None)
    {
        Atom xa_prop = XInternAtom(xdisplay, "_NET_WM_STATE_SKIP_TASKBAR", False);

        XChangeProperty(xdisplay, xwindow, xa, XA_ATOM, 32,
                        PropModeAppend,
                        (unsigned char *)&xa_prop, 1);
    }

    xa = XInternAtom(xdisplay, "_NET_WM_STATE", False);
    if (xa != None)
    {
        Atom xa_prop = XInternAtom(xdisplay, "_NET_WM_STATE_SKIP_PAGER", False);

        XChangeProperty(xdisplay, xwindow, xa, XA_ATOM, 32,
                        PropModeAppend,
                        (unsigned char *)&xa_prop, 1);
    }

    ///
}
