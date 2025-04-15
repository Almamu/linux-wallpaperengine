#include "CShaderConstant.h"

#include <utility>

using namespace WallpaperEngine::Core::Objects::Effects::Constants;

CShaderConstant::CShaderConstant (std::string type) :
    m_type (std::move(type)) {}

const std::string& CShaderConstant::getType () const {
    return this->m_type;
}
