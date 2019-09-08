#pragma once

#include "CShaderConstant.h"

#include <string>
#include <irrlicht/irrlicht.h>

namespace WallpaperEngine::Core::Objects::Effects
{
    class CShaderConstantFloat : public CShaderConstant
    {
    public:
        CShaderConstantFloat (irr::f32 value);

        irr::f32* getValue ();

    protected:
        irr::f32 m_value;

        static const std::string Type;
    };
}
