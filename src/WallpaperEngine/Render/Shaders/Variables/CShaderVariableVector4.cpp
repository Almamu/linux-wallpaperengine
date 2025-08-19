#include "CShaderVariableVector4.h"

#include <utility>

using namespace WallpaperEngine::Render::Shaders::Variables;

CShaderVariableVector4::CShaderVariableVector4 (const glm::vec4& defaultValue, const std::string& name) :
    CShaderVariable (defaultValue) {
    this->setName (name);
}
