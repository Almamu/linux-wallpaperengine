#include "CShaderConstantVector2.h"

using namespace WallpaperEngine::Core::Objects::Effects::Constants;

CShaderConstantVector2::CShaderConstantVector2 (glm::vec2 value) :
    CShaderConstant (Type) {
    this->update (value);
}

std::string CShaderConstantVector2::toString () const {
    std::string result = "(";

    result.append (std::to_string (this->getVec2 ().x));
    result.append (",");
    result.append (std::to_string (this->getVec2 ().y));
    result.append (")");

    return result;
}

const std::string CShaderConstantVector2::Type = "vec2";