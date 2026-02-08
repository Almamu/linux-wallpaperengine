#include "GLFWOutputViewport.h"

#include <utility>

using namespace WallpaperEngine::Render::Drivers::Output;

GLFWOutputViewport::GLFWOutputViewport (glm::ivec4 viewport, std::string name) :
    OutputViewport (viewport, std::move (name)) { }

void GLFWOutputViewport::makeCurrent () { }

void GLFWOutputViewport::swapOutput () { }