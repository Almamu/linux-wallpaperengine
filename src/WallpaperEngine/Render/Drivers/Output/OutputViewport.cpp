#include "OutputViewport.h"

#include <utility>

using namespace WallpaperEngine::Render::Drivers::Output;

OutputViewport::OutputViewport (const glm::ivec4 viewport, std::string name, const bool single) :
    viewport (viewport),
    name (std::move (name)),
    single (single) {}
