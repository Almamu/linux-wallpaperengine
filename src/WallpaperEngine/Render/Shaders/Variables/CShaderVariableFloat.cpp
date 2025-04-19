#include "CShaderVariableFloat.h"

#include <utility>

using namespace WallpaperEngine::Render::Shaders::Variables;

CShaderVariableFloat::CShaderVariableFloat (float defaultValue) :
    CShaderVariable (Type) {
    this->update (defaultValue);
}

CShaderVariableFloat::CShaderVariableFloat (float defaultValue, const std::string& name) :
    CShaderVariable (Type) {
    this->setName (name);
    this->update (defaultValue);
}

const std::string CShaderVariableFloat::Type = "float";