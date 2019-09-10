#include "CShaderVariableFloatPointer.h"

#include <utility>

using namespace WallpaperEngine::Render::Shaders::Variables;


CShaderVariableFloatPointer::CShaderVariableFloatPointer(irr::f32* value, int size) :
    m_size (size),
    CShaderVariable (value, nullptr, Type)
{
}

CShaderVariableFloatPointer::CShaderVariableFloatPointer(irr::f32* value, int size, std::string name) :
    m_size (size),
    CShaderVariable (value, nullptr, Type)
{
    this->setName (std::move(name));
}

const int CShaderVariableFloatPointer::getSize () const
{
    return this->m_size;
}

const std::string CShaderVariableFloatPointer::Type = "pointer";