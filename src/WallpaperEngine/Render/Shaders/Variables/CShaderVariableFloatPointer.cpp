#include "CShaderVariableFloatPointer.h"

#include <utility>

using namespace WallpaperEngine::Render::Shaders::Variables;


CShaderVariableFloatPointer::CShaderVariableFloatPointer(float* value) :
    CShaderVariable (value, nullptr, Type)
{
}

CShaderVariableFloatPointer::CShaderVariableFloatPointer(float* value, std::string name) :
    CShaderVariable (value, nullptr, Type)
{
    this->setName (std::move(name));
}

const int CShaderVariableFloatPointer::getSize () const
{
    return 1;
}

const std::string CShaderVariableFloatPointer::Type = "pointer_float";