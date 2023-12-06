// This code is a modification of the original project that can be found at
// https://github.com/Lecrapouille/OpenGLCppWrapper

#include "GLWindow.hpp"
#include <iostream>
#include <cassert>

static void error_callback(int error, const char* description)
{
    std::cerr << error << ": " << description << std::endl;
}

GLWindow::GLWindow(uint32_t const width, uint32_t const height, const char *title, GLFWwindow* window)
  : m_width(width), m_height(height), m_title(title), m_window(window)
{}

void GLWindow::init()
{
    // // Initialize glfw3
    // glfwSetErrorCallback(error_callback);
    // if (!glfwInit())
    // {
    //     std::cerr << "glfwInit: failed" << std::endl;
    //     exit(1);
    // }

    // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    // glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    // glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    // m_window = glfwCreateWindow(static_cast<int>(m_width),
    //                             static_cast<int>(m_height),
    //                             m_title.c_str(), nullptr, nullptr);
    // if (!m_window)
    // {
    //     std::cerr << "glfwCreateWindow: failed" << std::endl;
    //     glfwTerminate();
    //     exit(1);
    // }

    // glfwMakeContextCurrent(m_window);
    // glfwSwapInterval(1); // Enable vsync
    glfwSetWindowUserPointer(m_window, this); // Pass m_window in callbacks

    // // Initialize GLEW
    // glewExperimental = GL_TRUE; // stops glew crashing on OSX :-/
    // if (GLEW_OK != glewInit())
    // {
    //     std::cerr << "glewInit: failed" << std::endl;
    //     exit(1);
    // }

    // // Print out some info about the graphics drivers
    // std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;
    // std::cout << "GLSL version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    // std::cout << "Vendor: " << glGetString(GL_VENDOR) << std::endl;
    // std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;

    // // Make sure OpenGL version 3.2 API is available
    // if (!GLEW_VERSION_3_2)
    // {
    //     std::cerr << "OpenGL 3.2 API is not available!" << std::endl;
    // }
}

GLWindow::~GLWindow()
{
    if (nullptr != m_window)
        glfwDestroyWindow(m_window);
    glfwTerminate();
}

bool GLWindow::start() //Don't use
{
    init();

    if (!setup())
        return false;

    while (!glfwWindowShouldClose(m_window))
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (!update())
            return false;

        glfwSwapBuffers(m_window);
        glfwPollEvents();
    }

    return true;
}
