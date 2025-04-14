#include "CShaderConstant.h"

using namespace WallpaperEngine::Core::Objects::Effects::Constants;

CShaderConstant::CShaderConstant (std::string type) :
    m_type (type) {}

const std::string& CShaderConstant::getType () const {
    return this->m_type;
}
