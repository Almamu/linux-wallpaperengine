#include "COpenGLDriver.h"
#include "common.h"
#include <FreeImage.h>

using namespace WallpaperEngine::Render::Drivers;

COpenGLDriver::COpenGLDriver (const char* windowTitle) :
    m_frameCounter (0)
{
    // initialize glfw
    if (glfwInit () == GLFW_FALSE)
        sLog.exception ("Failed to initialize glfw");

    // set some window hints (opengl version to be used)
    glfwWindowHint (GLFW_SAMPLES, 4);
    glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint (GLFW_VISIBLE, GLFW_FALSE);

    if (DEBUG)
        glfwWindowHint (GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

    // create window, size doesn't matter as long as we don't show it
    this->m_window = glfwCreateWindow (640, 480, windowTitle, nullptr, nullptr);

    if (this->m_window == nullptr)
        sLog.exception ("Cannot create window");

    // make context current, required for glew initialization
    glfwMakeContextCurrent (this->m_window);

    // initialize glew for rendering
    GLenum result = glewInit ();

    if (result != GLEW_OK)
        sLog.error("Failed to initialize GLEW: ", glewGetErrorString (result));

    // initialize free image
    FreeImage_Initialise (TRUE);
}

COpenGLDriver::~COpenGLDriver ()
{
    glfwTerminate ();
    FreeImage_DeInitialise();
}

float COpenGLDriver::getRenderTime ()
{
    return (float) glfwGetTime ();
}

bool COpenGLDriver::closeRequested ()
{
    return glfwWindowShouldClose (this->m_window);
}

void COpenGLDriver::resizeWindow (glm::ivec2 size)
{
    glfwSetWindowSize (this->m_window, size.x, size.y);
}

void COpenGLDriver::showWindow ()
{
    glfwShowWindow (this->m_window);
}

void COpenGLDriver::hideWindow ()
{
    glfwHideWindow (this->m_window);
}

glm::ivec2 COpenGLDriver::getFramebufferSize ()
{
    glm::ivec2 size;

    glfwGetFramebufferSize (this->m_window, &size.x, &size.y);

    return size;
}

void COpenGLDriver::swapBuffers ()
{
    // do buffer swapping first
    glfwSwapBuffers (this->m_window);
    // poll for events
    glfwPollEvents ();
    // increase frame counter
    this->m_frameCounter ++;
}

uint32_t COpenGLDriver::getFrameCounter ()
{
    return this->m_frameCounter;
}

GLFWwindow* COpenGLDriver::getWindow ()
{
    return this->m_window;
}