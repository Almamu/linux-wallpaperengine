#include "EmbeddedOutputViewport.h"

#include <utility>

using namespace WallpaperEngine::Render::Drivers::Output;

EmbeddedOutputViewport::EmbeddedOutputViewport (glm::ivec4 viewport, std::string name) :
    OutputViewport (viewport, std::move (name)) { }

void EmbeddedOutputViewport::makeCurrent () { }

void EmbeddedOutputViewport::swapOutput () { }
