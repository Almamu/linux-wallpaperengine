#pragma once

#include "CShaderConstant.h"

#include <string>
#include <irrlicht/irrlicht.h>

namespace WallpaperEngine::Core::Objects::Effects
{
    class CShaderConstantInteger : public CShaderConstant
    {
    public:
        CShaderConstantInteger (irr::s32 value);

        irr::u32* getValue ();

    protected:
        irr::u32 m_value;

        static const std::string Type;
    };
}
