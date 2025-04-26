#include "CShaderVariableVector3.h"

using namespace WallpaperEngine::Render::Shaders::Variables;

CShaderVariableVector3::CShaderVariableVector3 (const glm::vec3& defaultValue) :
    CShaderVariable () {
    this->update (defaultValue);
}

CShaderVariableVector3::CShaderVariableVector3 (const glm::vec3& defaultValue, const std::string& name) :
    CShaderVariable () {
    this->setName (name);
    this->update (defaultValue);
}
