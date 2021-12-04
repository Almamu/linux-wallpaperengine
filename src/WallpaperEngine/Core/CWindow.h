#pragma once

#define GLFW_EXPOSE_NATIVE_X11
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"
#include <X11/Xatom.h>

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
