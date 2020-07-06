#include "CShaderVariableVector2Pointer.h"

#include <utility>

using namespace WallpaperEngine::Render::Shaders::Variables;


CShaderVariableVector2Pointer::CShaderVariableVector2Pointer(irr::core::vector2df* value) :
    CShaderVariable (value, nullptr, Type)
{
}

CShaderVariableVector2Pointer::CShaderVariableVector2Pointer(irr::core::vector2df* value, std::string name) :
    CShaderVariable (value, nullptr, Type)
{
    this->setName (std::move(name));
}

const int CShaderVariableVector2Pointer::getSize () const
{
    return 2;
}

const std::string CShaderVariableVector2Pointer::Type = "pointer_vector2";