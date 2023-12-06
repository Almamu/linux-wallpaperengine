// This code is a modification of the original project that can be found at
// https://github.com/Lecrapouille/OpenGLCppWrapper

#ifndef GLWINDOW_HPP
#  define GLWINDOW_HPP

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <string>

// *****************************************************************************
//! \brief Base class for creating OpenGL window. This class create the OpenGL
//! context, create the windows and call private virtual methods setup() and
//! update(). This must be derived to implement setup() and update().
// *****************************************************************************
class GLWindow
{
public:

    GLWindow(uint32_t const width, uint32_t const height, const char *title, GLFWwindow* window);
    virtual ~GLWindow();

    //! \brief call setup() once and if succeeded call update() within a runtime
    //! loop. Return false in case of failure.
    bool start();

    void init();
private:

    //! \brief Implement the init for your application. Return false in case of failure.
    virtual bool setup() = 0;

    //! \brief Implement the update for your application. Return false in case of failure.
    virtual bool update() = 0;

protected:

    //! \brief The OpenGL whindows holding the context
    GLFWwindow *m_window = nullptr;
    uint32_t m_width;
    uint32_t m_height;
    std::string m_title;
};

#endif
