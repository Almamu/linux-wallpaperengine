#include "CGLFWOutputViewport.h"

#include <utility>

using namespace WallpaperEngine::Render::Drivers::Output;

CGLFWOutputViewport::CGLFWOutputViewport (glm::ivec4 viewport, std::string name) :
    COutputViewport (viewport, std::move (name)) {}

void CGLFWOutputViewport::makeCurrent () {}

void CGLFWOutputViewport::swapOutput () {}