#pragma once

#include "CShaderVariable.h"

#include <irrlicht/irrlicht.h>

namespace WallpaperEngine::Render::Shaders::Variables
{
    class CShaderVariableInteger : public CShaderVariable
    {
    public:
        explicit CShaderVariableInteger (irr::s32 defaultValue);
        CShaderVariableInteger (irr::s32 defaultValue, std::string name);

        const int getSize () const override;

        static const std::string Type;

        void setValue (irr::s32 value);

    private:
        irr::s32 m_defaultValue;
        irr::s32 m_value;
    };
}
