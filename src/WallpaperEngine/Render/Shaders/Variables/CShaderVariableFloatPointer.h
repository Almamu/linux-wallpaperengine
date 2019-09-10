#pragma once

#include "CShaderVariable.h"

#include <irrlicht/irrlicht.h>

namespace WallpaperEngine::Render::Shaders::Variables
{
    class CShaderVariableFloatPointer : public CShaderVariable
    {
    public:
        CShaderVariableFloatPointer (irr::f32* value, int size);
        CShaderVariableFloatPointer (irr::f32* value, int size, std::string name);

        const int getSize () const override;

        static const std::string Type;
    private:
        int m_size;
    };
}
