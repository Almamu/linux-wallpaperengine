#include "CShaderConstantVector4.h"

using namespace WallpaperEngine::Core::Objects::Effects::Constants;

CShaderConstantVector4::CShaderConstantVector4 (glm::vec4 value) :
    CShaderConstant (Type) {
    this->update (value);
}

std::string CShaderConstantVector4::toString () const {
    std::string result = "(";

    result.append (std::to_string (this->getVec4 ().x));
    result.append (",");
    result.append (std::to_string (this->getVec4 ().y));
    result.append (",");
    result.append (std::to_string (this->getVec4 ().z));
    result.append (",");
    result.append (std::to_string (this->getVec4 ().w));
    result.append (")");

    return result;
}

const std::string CShaderConstantVector4::Type = "vec4";