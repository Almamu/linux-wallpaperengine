#include "OutputViewport.h"

#include <utility>

using namespace WallpaperEngine::Render::Drivers::Output;

OutputViewport::OutputViewport (glm::ivec4 viewport, std::string name, bool single) :
    viewport (viewport),
    name (std::move (name)),
    single (single) {}
