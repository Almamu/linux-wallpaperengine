#pragma once

#include "CShaderParameter.h"

#include <irrlicht/irrlicht.h>

namespace WallpaperEngine::Render::Shaders::Parameters
{
    class CShaderParameterInteger : public CShaderParameter
    {
    public:
        CShaderParameterInteger (irr::s32 defaultValue);

        const int getSize () const override;

        static const std::string Type;

        void setValue (irr::s32 value);

    private:
        irr::s32 m_defaultValue;
        irr::s32 m_value;
    };
}
