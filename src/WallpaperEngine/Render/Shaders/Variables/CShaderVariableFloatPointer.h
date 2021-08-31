#pragma once

#include "CShaderVariable.h"

namespace WallpaperEngine::Render::Shaders::Variables
{
    class CShaderVariableFloatPointer : public CShaderVariable
    {
    public:
        explicit CShaderVariableFloatPointer (float* value);
        CShaderVariableFloatPointer (float* value, std::string name);

        const int getSize () const override;

        static const std::string Type;
    private:
    };
}
