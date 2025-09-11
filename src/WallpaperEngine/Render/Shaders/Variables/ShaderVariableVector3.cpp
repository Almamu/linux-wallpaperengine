#include "ShaderVariableVector3.h"

using namespace WallpaperEngine::Render::Shaders::Variables;

ShaderVariableVector3::ShaderVariableVector3 (const glm::vec3& defaultValue, const std::string& name) :
    ShaderVariable (defaultValue) {
    this->setName (name);
}
