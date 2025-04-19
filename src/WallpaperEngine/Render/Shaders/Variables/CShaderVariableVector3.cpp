#include "CShaderVariableVector3.h"

using namespace WallpaperEngine::Render::Shaders::Variables;

CShaderVariableVector3::CShaderVariableVector3 (const glm::vec3& defaultValue) :
    CShaderVariable (Type) {
    this->update (defaultValue);
}

CShaderVariableVector3::CShaderVariableVector3 (const glm::vec3& defaultValue, const std::string& name) :
    CShaderVariable (Type) {
    this->setName (name);
    this->update (defaultValue);
}

const std::string CShaderVariableVector3::Type = "vec3";