#pragma once

#define GLFW_EXPOSE_NATIVE_X11
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"
#include <X11/Xatom.h>

#ifdef LONG64
typedef unsigned long XCARD64;
typedef unsigned int XCARD32;
#else
typedef unsigned long long XCARD64;
typedef unsigned long XCARD32;
#endif

namespace WallpaperEngine::Core
{
    class CWindow
    {
    public:
        static void MakeWindowsDesktop(GLFWwindow *window);

    protected:
    private:
    };
}
