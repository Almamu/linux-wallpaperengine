#include "CShaderVariableInteger.h"

#include <utility>

using namespace WallpaperEngine::Render::Shaders::Variables;

CShaderVariableInteger::CShaderVariableInteger (int defaultValue, const std::string& name) :
    CShaderVariable (defaultValue) {
    this->setName (name);
}
