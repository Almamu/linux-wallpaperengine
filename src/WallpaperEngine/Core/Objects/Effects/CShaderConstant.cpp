#include "CShaderConstant.h"

using namespace WallpaperEngine::Core::Objects::Effects;

CShaderConstant::CShaderConstant (std::string type) :
    m_type (std::move(type))
{
}