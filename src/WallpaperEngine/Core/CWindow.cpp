#include "CWindow.h"

using namespace WallpaperEngine::Core;

#define _NET_WM_STATE_REMOVE 0 /* remove/unset property */
#define _NET_WM_STATE_ADD 1    /* add/set property */
#define _NET_WM_STATE_TOGGLE 2 /* toggle property  */

Status x11_sendEvent(Display *display, Window xid, Atom type, Atom prop, Atom action)
{
    XEvent event;
    event.xclient.type = ClientMessage;
    event.xclient.serial = 0;
    event.xclient.send_event = True;
    event.xclient.display = display;
    event.xclient.window = xid;
    event.xclient.message_type = type;
    event.xclient.format = 32;

    event.xclient.data.l[0] = action;
    event.xclient.data.l[1] = prop;
    event.xclient.data.l[2] = 0; // unused.
    event.xclient.data.l[3] = 0;
    event.xclient.data.l[4] = 0;

    return XSendEvent(display, DefaultRootWindow(display), False,
                      SubstructureRedirectMask | SubstructureNotifyMask, &event);
}

void CWindow::MakeWindowsDesktop(GLFWwindow *window)
{

    Window xwindow = glfwGetX11Window(window);
    Display *xdisplay = glfwGetX11Display();

    Atom xa = XInternAtom(xdisplay, "_NET_WM_WINDOW_TYPE", False);
    Atom type = XInternAtom(xdisplay, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
    XChangeProperty(xdisplay, xwindow,
                    xa, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)&type, 1);

    x11_sendEvent(xdisplay, xwindow, XInternAtom(xdisplay, "_NET_WM_DESKTOP", False), 0xFFFFFFFF, _NET_WM_STATE_ADD);

    x11_sendEvent(xdisplay, xwindow, XInternAtom(xdisplay, "_NET_WM_STATE", False), XInternAtom(xdisplay, "_NET_WM_STATE_STICKY", False), _NET_WM_STATE_ADD);

    x11_sendEvent(xdisplay, xwindow, XInternAtom(xdisplay, "_WIN_LAYER", False), 0, _NET_WM_STATE_ADD);

    x11_sendEvent(xdisplay, xwindow, XInternAtom(xdisplay, "_NET_WM_STATE", False), XInternAtom(xdisplay, "_NET_WM_STATE_BELOW", False), _NET_WM_STATE_ADD);

    x11_sendEvent(xdisplay, xwindow, XInternAtom(xdisplay, "_NET_WM_STATE", False), XInternAtom(xdisplay, "_NET_WM_STATE_SKIP_TASKBAR", False), _NET_WM_STATE_ADD);

    x11_sendEvent(xdisplay, xwindow, XInternAtom(xdisplay, "_NET_WM_STATE", False), XInternAtom(xdisplay, "_NET_WM_STATE_SKIP_PAGER", False), _NET_WM_STATE_ADD);

    XLowerWindow(xdisplay, xwindow);

    ///
}
