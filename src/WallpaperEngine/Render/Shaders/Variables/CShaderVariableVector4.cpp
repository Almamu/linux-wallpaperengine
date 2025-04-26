#include "CShaderVariableVector4.h"

#include <utility>

using namespace WallpaperEngine::Render::Shaders::Variables;

CShaderVariableVector4::CShaderVariableVector4 (const glm::vec4& defaultValue) :
    CShaderVariable () {
    this->update (defaultValue);
}

CShaderVariableVector4::CShaderVariableVector4 (const glm::vec4& defaultValue, const std::string& name) :
    CShaderVariable () {
    this->setName (name);
    this->update (defaultValue);
}
