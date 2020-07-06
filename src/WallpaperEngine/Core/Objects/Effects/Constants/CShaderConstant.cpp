#include "CShaderConstant.h"

using namespace WallpaperEngine::Core::Objects::Effects::Constants;

CShaderConstant::CShaderConstant (std::string type) :
    m_type (std::move(type))
{
}