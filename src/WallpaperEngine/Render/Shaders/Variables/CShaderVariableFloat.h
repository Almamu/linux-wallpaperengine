#pragma once

#include "CShaderVariable.h"

#include <irrlicht/irrlicht.h>

namespace WallpaperEngine::Render::Shaders::Variables
{
    class CShaderVariableFloat : public CShaderVariable
    {
    public:
        explicit CShaderVariableFloat (irr::f32 defaultValue);
        CShaderVariableFloat (irr::f32 defaultValue, std::string name);

        const int getSize () const override;

        void setValue (irr::f32 value);

        static const std::string Type;

    private:
        irr::f32 m_defaultValue;
        irr::f32 m_value;
    };
}
