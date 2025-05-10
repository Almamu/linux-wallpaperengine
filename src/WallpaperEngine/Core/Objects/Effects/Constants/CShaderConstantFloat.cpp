#include "CShaderConstantFloat.h"

using namespace WallpaperEngine::Core::Objects::Effects::Constants;

CShaderConstantFloat::CShaderConstantFloat (float value) {
    this->update (value);
}

std::string CShaderConstantFloat::toString () const {
    return std::to_string (this->getFloat ());
}
