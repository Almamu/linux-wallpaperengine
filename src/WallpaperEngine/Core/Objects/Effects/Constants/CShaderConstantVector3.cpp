#include "CShaderConstantVector3.h"

using namespace WallpaperEngine::Core::Objects::Effects::Constants;

CShaderConstantVector3::CShaderConstantVector3 (glm::vec3 value) {
    this->update (value);
}

std::string CShaderConstantVector3::toString () const {
    std::string result = "(";

    result.append (std::to_string (this->getVec3 ().x));
    result.append (",");
    result.append (std::to_string (this->getVec3 ().y));
    result.append (",");
    result.append (std::to_string (this->getVec3 ().z));
    result.append (")");

    return result;
}
