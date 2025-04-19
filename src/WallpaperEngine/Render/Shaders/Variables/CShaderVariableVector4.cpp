#include "CShaderVariableVector4.h"

#include <utility>

using namespace WallpaperEngine::Render::Shaders::Variables;

CShaderVariableVector4::CShaderVariableVector4 (const glm::vec4& defaultValue) :
    CShaderVariable (Type) {
    this->update (defaultValue);
}

CShaderVariableVector4::CShaderVariableVector4 (const glm::vec4& defaultValue, const std::string& name) :
    CShaderVariable (Type) {
    this->setName (name);
    this->update (defaultValue);
}

const std::string CShaderVariableVector4::Type = "vec4";