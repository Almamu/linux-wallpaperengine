#include <GLFW/glfw3.h>
#include "CGLFWWindowOutput.h"
#include "WallpaperEngine/Logging/CLog.h"
#include "CX11OutputViewport.h"

#include <unistd.h>

#define FULLSCREEN_CHECK_WAIT_TIME 250

using namespace WallpaperEngine::Render::Drivers::Output;

CGLFWWindowOutput::CGLFWWindowOutput (CApplicationContext& context, CVideoDriver& driver) :
    COutput (context, driver)
{
    if (
        this->m_context.settings.render.mode != Application::CApplicationContext::NORMAL_WINDOW &&
        this->m_context.settings.render.mode != Application::CApplicationContext::EXPLICIT_WINDOW)
        sLog.exception ("Initializing window output when not in output mode, how did you get here?!");

    // window should be visible
    driver.showWindow ();

    if (this->m_context.settings.render.mode == Application::CApplicationContext::EXPLICIT_WINDOW)
    {
        this->m_fullWidth = this->m_context.settings.render.window.geometry.z;
        this->m_fullHeight = this->m_context.settings.render.window.geometry.w;
        this->repositionWindow ();
    }
    else
    {
        // take the size from the driver (default window size)
        this->m_fullWidth = this->m_driver.getFramebufferSize ().x;
        this->m_fullHeight = this->m_driver.getFramebufferSize ().y;
    }

    // register the default viewport
    this->m_viewports ["default"] = new CX11OutputViewport {{0, 0, this->m_fullWidth, this->m_fullHeight}, "default"};
}

void CGLFWWindowOutput::repositionWindow ()
{
    // reposition the window
    this->m_driver.resizeWindow (this->m_context.settings.render.window.geometry);
}

void CGLFWWindowOutput::reset ()
{
    if (this->m_context.settings.render.mode == Application::CApplicationContext::EXPLICIT_WINDOW)
        this->repositionWindow ();
}

bool CGLFWWindowOutput::renderVFlip () const
{
    return true;
}

bool CGLFWWindowOutput::renderMultiple () const
{
    return false;
}

bool CGLFWWindowOutput::haveImageBuffer () const
{
    return false;
}

void* CGLFWWindowOutput::getImageBuffer () const
{
    return nullptr;
}
void CGLFWWindowOutput::updateRender () const
{
    if (this->m_context.settings.render.mode != Application::CApplicationContext::NORMAL_WINDOW)
        return;

    // take the size from the driver (default window size)
    this->m_fullWidth = this->m_driver.getFramebufferSize ().x;
    this->m_fullHeight = this->m_driver.getFramebufferSize ().y;

    // update the default viewport
    this->m_viewports ["default"]->viewport = {0, 0, this->m_fullWidth, this->m_fullHeight};

    // check for fullscreen windows and wait until there's none fullscreen
    while (this->m_driver.getFullscreenDetector ().anythingFullscreen () && this->m_context.state.general.keepRunning)
        usleep (FULLSCREEN_CHECK_WAIT_TIME);
}