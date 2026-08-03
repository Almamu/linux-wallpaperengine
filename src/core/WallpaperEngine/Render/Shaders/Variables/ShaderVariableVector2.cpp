#include "ShaderVariableVector2.h"

#include <utility>

using namespace WallpaperEngine::Render::Shaders::Variables;

ShaderVariableVector2::ShaderVariableVector2 (const glm::vec2& defaultValue, const std::string& name) :
    ShaderVariable (defaultValue) {
    this->setName (name);
}
