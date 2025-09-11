#include "ShaderVariableInteger.h"

#include <utility>

using namespace WallpaperEngine::Render::Shaders::Variables;

ShaderVariableInteger::ShaderVariableInteger (int defaultValue, const std::string& name) :
    ShaderVariable (defaultValue) {
    this->setName (name);
}
