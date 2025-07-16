#include "CShaderVariableFloat.h"

#include <utility>

using namespace WallpaperEngine::Render::Shaders::Variables;

CShaderVariableFloat::CShaderVariableFloat (float defaultValue) :
    CShaderVariable () {
    this->update (defaultValue);
}

CShaderVariableFloat::CShaderVariableFloat (float defaultValue, const std::string& name) :
    CShaderVariable () {
    this->setName (name);
    this->update (defaultValue);
}
