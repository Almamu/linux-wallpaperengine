#include "CShaderVariableVector2.h"

#include <utility>

using namespace WallpaperEngine::Render::Shaders::Variables;

CShaderVariableVector2::CShaderVariableVector2 (const glm::vec2& defaultValue) :
    CShaderVariable (Type) {
    this->update (defaultValue);
}

CShaderVariableVector2::CShaderVariableVector2 (const glm::vec2& defaultValue, const std::string& name) :
    CShaderVariable (Type) {
    this->setName (name);
    this->update (defaultValue);
}

const std::string CShaderVariableVector2::Type = "vec2";