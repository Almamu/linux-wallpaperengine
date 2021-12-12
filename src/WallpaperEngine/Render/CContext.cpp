#include <iostream>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include <GL/glew.h>
#include <GL/glx.h>

#include "CContext.h"
#include "CVideo.h"

using namespace WallpaperEngine::Render;

CContext::CContext(std::vector<std::string> screens) : m_wallpaper(nullptr),
                                                       m_screens(std::move(screens)),
                                                       m_isRootWindow(m_screens.empty() == false),
                                                       m_defaultViewport({0, 0, 1920, 1080})
{
    this->initializeViewports();
}

void CContext::initializeViewports()
{
    if (this->m_isRootWindow == false || this->m_screens.empty() == true)
        return;

    Display *display = XOpenDisplay(nullptr);

    int xrandr_result, xrandr_error;

    if (!XRRQueryExtension(display, &xrandr_result, &xrandr_error))
    {
        std::cerr << "XRandr is not present, cannot detect specified screens, running in window mode" << std::endl;
        return;
    }

    int fullWidth = DisplayWidth(display, DefaultScreen(display));
    int fullHeight = DisplayHeight(display, DefaultScreen(display));
    XRRScreenResources *screenResources = XRRGetScreenResources(display, DefaultRootWindow(display));

    // there are some situations where xrandr returns null (like screen not using the extension)
    if (screenResources == nullptr)
        return;

    for (int i = 0; i < screenResources->noutput; i++)
    {
        XRROutputInfo *info = XRRGetOutputInfo(display, screenResources, screenResources->outputs[i]);

        // there are some situations where xrandr returns null (like screen not using the extension)
        if (info == nullptr)
            continue;

        auto cur = this->m_screens.begin();
        auto end = this->m_screens.end();

        for (; cur != end; cur++)
        {
            if (info->connection == RR_Connected && strcmp(info->name, (*cur).c_str()) == 0)
            {
                XRRCrtcInfo *crtc = XRRGetCrtcInfo(display, screenResources, info->crtc);

                std::cout << "Found requested screen: " << info->name << " -> " << crtc->x << "x" << crtc->y << ":" << crtc->width << "x" << crtc->height << std::endl;

                glm::vec4 viewport = {
                    crtc->x, fullHeight - (crtc->y + crtc->height), crtc->width, crtc->height};

                this->m_viewports.push_back(viewport);

                XRRFreeCrtcInfo(crtc);
            }
        }

        XRRFreeOutputInfo(info);
    }

    XRRFreeScreenResources(screenResources);

    // set the
    // glfwWindowHintPointer (GLFW_NATIVE_PARENT_HANDLE, reinterpret_cast <void*> (DefaultRootWindow (display)));
}

void CContext::render()
{
    if (this->m_wallpaper == nullptr)
        return;

    if (this->m_viewports.empty() == false)
    {
        bool firstFrame = true;
        bool renderFrame = true;
        auto cur = this->m_viewports.begin();
        auto end = this->m_viewports.end();

        for (; cur != end; cur++)
        {
            this->m_wallpaper->render(*cur, renderFrame, firstFrame);
            // scenes need to render a new frame for each viewport as they produce different results
            // but videos should only be rendered once per group of viewports
            firstFrame = false;
            renderFrame = !this->m_wallpaper->is<CVideo>();
        }
    }
    else
        this->m_wallpaper->render(this->m_defaultViewport);
}

void CContext::setWallpaper(CWallpaper *wallpaper)
{
    this->m_wallpaper = wallpaper;
}

void CContext::setDefaultViewport(glm::vec4 defaultViewport)
{
    this->m_defaultViewport = defaultViewport;
}

CMouseInput *CContext::getMouse() const
{
    return this->m_mouse;
}

void CContext::setMouse(CMouseInput *mouse)
{
    this->m_mouse = mouse;
}
