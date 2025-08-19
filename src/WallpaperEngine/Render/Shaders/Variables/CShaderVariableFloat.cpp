#include "CShaderVariableFloat.h"

#include <utility>

using namespace WallpaperEngine::Render::Shaders::Variables;

CShaderVariableFloat::CShaderVariableFloat (float defaultValue, const std::string& name) :
    CShaderVariable (defaultValue) {
    this->setName (name);
}
