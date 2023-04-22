#include "COutputViewport.h"

#include <utility>

using namespace WallpaperEngine::Render::Drivers::Output;

COutputViewport::COutputViewport (glm::ivec4 viewport, std::string name, bool single) :
    viewport (viewport),
    name (std::move(name)),
    single (single)
{
}

COutputViewport::~COutputViewport ()
{
}