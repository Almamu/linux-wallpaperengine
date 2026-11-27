#pragma once

#include <stdexcept>

namespace WallpaperEngine::Render::Shaders {
/** The GLSL translator explicitly rejected the stages for this pass. */
class ShaderTranslationError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
}
