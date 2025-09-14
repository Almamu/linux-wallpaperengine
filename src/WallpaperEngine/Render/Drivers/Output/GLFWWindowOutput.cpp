#include "GLFWOutputViewport.h"
#include "GLFWWindowOutput.h"
#include "WallpaperEngine/Logging/Log.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <unistd.h>

using namespace WallpaperEngine::Render::Drivers::Output;

GLFWWindowOutput::GLFWWindowOutput (ApplicationContext& context, VideoDriver& driver) : Output (context, driver) {
    if (this->m_context.settings.render.mode != Application::ApplicationContext::NORMAL_WINDOW &&
        this->m_context.settings.render.mode != Application::ApplicationContext::EXPLICIT_WINDOW)
        sLog.exception ("Initializing window output when not in output mode, how did you get here?!");

    // window should be visible
    driver.showWindow ();

    if (this->m_context.settings.render.mode == Application::ApplicationContext::EXPLICIT_WINDOW) {
        this->m_fullWidth = this->m_context.settings.render.window.geometry.z;
        this->m_fullHeight = this->m_context.settings.render.window.geometry.w;
        this->repositionWindow ();
    } else {
        // take the size from the driver (default window size)
        this->m_fullWidth = this->m_driver.getFramebufferSize ().x;
        this->m_fullHeight = this->m_driver.getFramebufferSize ().y;
    }

    // register the default viewport
    this->m_viewports ["default"] = new GLFWOutputViewport {{0, 0, this->m_fullWidth, this->m_fullHeight}, "default"};
}

void GLFWWindowOutput::repositionWindow () const {
    // reposition the window
    this->m_driver.resizeWindow (this->m_context.settings.render.window.geometry);
}

void GLFWWindowOutput::reset () {
    if (this->m_context.settings.render.mode == Application::ApplicationContext::EXPLICIT_WINDOW)
        this->repositionWindow ();
}

bool GLFWWindowOutput::renderVFlip () const {
    return true;
}

bool GLFWWindowOutput::renderMultiple () const {
    return false;
}

bool GLFWWindowOutput::haveImageBuffer () const {
    return false;
}

void* GLFWWindowOutput::getImageBuffer () const {
    return nullptr;
}

uint32_t GLFWWindowOutput::getImageBufferSize () const {
    return 0;
}

void GLFWWindowOutput::updateRender () const {
    if (this->m_context.settings.render.mode != Application::ApplicationContext::NORMAL_WINDOW)
        return;

    // take the size from the driver (default window size)
    this->m_fullWidth = this->m_driver.getFramebufferSize ().x;
    this->m_fullHeight = this->m_driver.getFramebufferSize ().y;

    // update the default viewport
    this->m_viewports ["default"]->viewport = {0, 0, this->m_fullWidth, this->m_fullHeight};
}