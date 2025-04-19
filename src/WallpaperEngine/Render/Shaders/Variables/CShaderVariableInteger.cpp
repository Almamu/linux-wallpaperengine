#include "CShaderVariableInteger.h"

#include <utility>

using namespace WallpaperEngine::Render::Shaders::Variables;

CShaderVariableInteger::CShaderVariableInteger (int32_t defaultValue) :
    CShaderVariable (Type) {
    this->update (defaultValue);
}

CShaderVariableInteger::CShaderVariableInteger (int32_t defaultValue, const std::string& name) :
    CShaderVariable (Type) {
    this->setName (name);
    this->update (defaultValue);
}


const std::string CShaderVariableInteger::Type = "int";