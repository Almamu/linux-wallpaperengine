#include "ShaderVariableVector4.h"

#include <utility>

using namespace WallpaperEngine::Render::Shaders::Variables;

ShaderVariableVector4::ShaderVariableVector4 (const glm::vec4& defaultValue, const std::string& name) :
    ShaderVariable (defaultValue) {
    this->setName (name);
}
