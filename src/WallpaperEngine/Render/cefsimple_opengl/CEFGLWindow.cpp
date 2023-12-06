// This code is a modification of the original project that can be found at
// https://github.com/if1live/cef-gl-example

#include "CEFGLWindow.hpp"
#include "GLCore.hpp"

//------------------------------------------------------------------------------
//! \brief Callback when the OpenGL base window has been resized. Dispatch this
//! event to all BrowserView.
//------------------------------------------------------------------------------
static void reshape_callback(GLFWwindow* ptr, int w, int h)
{
    assert(nullptr != ptr);
    CEFGLWindow* window = static_cast<CEFGLWindow*>(glfwGetWindowUserPointer(ptr));

    // Send screen size to browsers
    for (auto it: window->browsers())
    {
        it->reshape(w, h);
    }
}

//------------------------------------------------------------------------------
//! \brief Callback when the mouse has clicked inside the OpenGL base window.
//! Dispatch this event to all BrowserView.
//------------------------------------------------------------------------------
static void mouse_callback(GLFWwindow* ptr, int btn, int state, int /*mods*/)
{
    assert(nullptr != ptr);
    CEFGLWindow* window = static_cast<CEFGLWindow*>(glfwGetWindowUserPointer(ptr));

    // Send mouse click to browsers
    for (auto it: window->browsers())
    {
        it->mouseClick(CefBrowserHost::MouseButtonType(btn), state == GLFW_PRESS);
    }
}

//------------------------------------------------------------------------------
//! \brief Callback when the mouse has been displaced inside the OpenGL base
//! window. Dispatch this event to all BrowserView.
//------------------------------------------------------------------------------
static void motion_callback(GLFWwindow* ptr, double x, double y)
{
    assert(nullptr != ptr);
    CEFGLWindow* window = static_cast<CEFGLWindow*>(glfwGetWindowUserPointer(ptr));

    // Send mouse movement to browsers
    for (auto it: window->browsers())
    {
        it->mouseMove((int) x, (int) y);
    }
}

//------------------------------------------------------------------------------
//! \brief Callback when the keybaord has been pressed inside the OpenGL base
//! window. Dispatch this event to all BrowserView.
//------------------------------------------------------------------------------
static void keyboard_callback(GLFWwindow* ptr, int key, int /*scancode*/,
                              int action, int /*mods*/)
{
    assert(nullptr != ptr);
    CEFGLWindow* window = static_cast<CEFGLWindow*>(glfwGetWindowUserPointer(ptr));

    // Send key press to browsers
    for (auto it: window->browsers())
    {
        it->keyPress(key, (action == GLFW_PRESS));
    }
}

//------------------------------------------------------------------------------
CEFGLWindow::CEFGLWindow(uint32_t const width, uint32_t const height, const char *title, GLFWwindow* window)
    : GLWindow(width, height, title, window)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
}

//------------------------------------------------------------------------------
CEFGLWindow::~CEFGLWindow()
{
    m_browsers.clear();
    CefShutdown();
}

//------------------------------------------------------------------------------
std::weak_ptr<BrowserView> CEFGLWindow::createBrowser(const std::string &url)
{
    auto web_core = std::make_shared<BrowserView>(url);
    m_browsers.push_back(web_core);
    return web_core;
}

//------------------------------------------------------------------------------
void CEFGLWindow::removeBrowser(std::weak_ptr<BrowserView> web_core)
{
    auto elem = web_core.lock();
    if (elem)
    {
        auto found = std::find(m_browsers.begin(), m_browsers.end(), elem);
        if (found != m_browsers.end())
        {
            m_browsers.erase(found);
        }
    }
}

//------------------------------------------------------------------------------
bool CEFGLWindow::setup()
{
    this->init();
    std::vector<std::string> urls = {
        //"https://youtu.be/rRr19a08mHs",
        // "https://www.researchgate.net/profile/Philip-Polack/publication/318810853_The_kinematic_bicycle_model_A_consistent_model_for_planning_feasible_trajectories_for_autonomous_vehicles/links/5addcbc2a6fdcc29358b9c01/The-kinematic-bicycle-model-A-consistent-model-for-planning-feasible-trajectories-for-autonomous-vehicles.pdf",
        //"https://www.youtube.com/"
        "file:///home/kermit/.local/share/Steam/steamapps/workshop/content/431960/3026516467/Untitled-1.html"
    };

    // Create BrowserView
    for (auto const& url: urls)
    {
        auto browser = createBrowser(url);
        browser.lock()->reshape(m_width, m_height);
    }

    // Change viewports (vertical split)
    m_browsers[0]->viewport(0.0f, 0.0f, 1.0f, 1.0f);
    //m_browsers[1]->viewport(0.5f, 0.0f, 1.0f, 1.0f);

    // Do rotation animation
    // m_browsers[1]->m_fixed = false;

    // Windows events
    GLCHECK(glfwSetFramebufferSizeCallback(m_window, reshape_callback));
    GLCHECK(glfwSetKeyCallback(m_window, keyboard_callback));
    GLCHECK(glfwSetCursorPosCallback(m_window, motion_callback));
    GLCHECK(glfwSetMouseButtonCallback(m_window, mouse_callback));

    // Set OpenGL states
    //GLCHECK(glViewport(0, 0, m_width, m_height));
    // GLCHECK(glClearColor(0.0, 0.0, 0.0, 0.0));
    //GLCHECK(glEnable(GL_CULL_FACE));
    // GLCHECK(glEnable(GL_DEPTH_TEST));
    // GLCHECK(glDepthFunc(GL_LESS));
    // GLCHECK(glDisable(GL_BLEND));
    // GLCHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    return true;
}

//------------------------------------------------------------------------------
bool CEFGLWindow::update()
{
    GLCHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    for (auto it: m_browsers)
    {
        it->draw();
    }

    CefDoMessageLoopWork();
    return true;
}
