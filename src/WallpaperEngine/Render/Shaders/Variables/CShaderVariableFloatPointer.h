#pragma once

#include "CShaderVariable.h"

#include <irrlicht/irrlicht.h>

namespace WallpaperEngine::Render::Shaders::Variables
{
    class CShaderVariableFloatPointer : public CShaderVariable
    {
    public:
        explicit CShaderVariableFloatPointer (irr::f32* value);
        CShaderVariableFloatPointer (irr::f32* value, std::string name);

        const int getSize () const override;

        static const std::string Type;
    private:
    };
}
