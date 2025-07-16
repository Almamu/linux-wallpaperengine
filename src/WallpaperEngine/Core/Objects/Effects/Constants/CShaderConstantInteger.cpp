#include "CShaderConstantInteger.h"

using namespace WallpaperEngine::Core::Objects::Effects::Constants;

CShaderConstantInteger::CShaderConstantInteger (int32_t value) {
    this->update (value);
}

std::string CShaderConstantInteger::toString () const {
    return std::to_string (this->getInt ());
}
