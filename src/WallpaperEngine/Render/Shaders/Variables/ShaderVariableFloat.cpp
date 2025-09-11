#include "ShaderVariableFloat.h"

#include <utility>

using namespace WallpaperEngine::Render::Shaders::Variables;

ShaderVariableFloat::ShaderVariableFloat (float defaultValue, const std::string& name) :
    ShaderVariable (defaultValue) {
    this->setName (name);
}
