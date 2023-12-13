#include "CX11OutputViewport.h"

#include <utility>

using namespace WallpaperEngine::Render::Drivers::Output;

CX11OutputViewport::CX11OutputViewport (glm::ivec4 viewport, std::string name) :
    COutputViewport (viewport, std::move (name)) {}

void CX11OutputViewport::makeCurrent () {}

void CX11OutputViewport::swapOutput () {}