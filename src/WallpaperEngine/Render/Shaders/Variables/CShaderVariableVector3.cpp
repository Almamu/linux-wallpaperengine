#include "CShaderVariableVector3.h"

using namespace WallpaperEngine::Render::Shaders::Variables;

CShaderVariableVector3::CShaderVariableVector3 (const glm::vec3& defaultValue, const std::string& name) :
    CShaderVariable (defaultValue) {
    this->setName (name);
}
