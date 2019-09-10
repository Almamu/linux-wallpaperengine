#pragma once

#include "CShaderParameter.h"

#include <irrlicht/irrlicht.h>

namespace WallpaperEngine::Render::Shaders::Parameters
{
    class CShaderParameterFloat : public CShaderParameter
    {
    public:
        CShaderParameterFloat (irr::f32 defaultValue);

        const int getSize () const override;

        void setValue (irr::f32 value);

        static const std::string Type;

    private:
        irr::f32 m_defaultValue;
        irr::f32 m_value;
    };
}
