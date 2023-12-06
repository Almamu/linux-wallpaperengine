// This code is a modification of the original project that can be found at
// https://github.com/Lecrapouille/OpenGLCppWrapper

#ifndef CEFGLWINDOW_HPP
#define CEFGLWINDOW_HPP

// Base application class
#include "GLWindow.hpp"
#include "BrowserView.hpp"

// ****************************************************************************
//! \brief Extend the OpenGL base window and add Chromium Embedded Framework
//! browser views.
// ****************************************************************************
class CEFGLWindow: public GLWindow
{
public:

    //! \brief Default construction: define the window dimension and title
    CEFGLWindow(uint32_t const width, uint32_t const height, const char *title, GLFWwindow* window);

    //! \brief Destructor
    ~CEFGLWindow();

    //! \brief Non const getter of the list of browsers
    inline std::vector<std::shared_ptr<BrowserView>>& browsers()
    {
        return m_browsers;
    }

// private: // Concrete implementation from GLWindow

    virtual bool setup() override;
    virtual bool update() override;

private:

    //! \brief Create a new browser view from a given URL
    std::weak_ptr<BrowserView> createBrowser(const std::string &url);

    //! \brief Destroy the given browser view.
    void removeBrowser(std::weak_ptr<BrowserView> web_core);

private:

    //! \brief List of BrowserView managed by createBrowser() and
    //! removeBrowser() methods.
    std::vector<std::shared_ptr<BrowserView>> m_browsers;
};

#endif // CEFGLWINDOW_HPP
