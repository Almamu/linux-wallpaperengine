#pragma once

#include "CShaderConstant.h"

#include <string>
#include <irrlicht/irrlicht.h>

namespace WallpaperEngine::Core::Objects::Effects
{
    class CShaderConstantVector3 : public CShaderConstant
    {
    public:
        CShaderConstantVector3 (irr::core::vector3df value);

        irr::core::vector3df* getValue ();

        static const std::string Type;
    protected:
        irr::core::vector3df m_value;
    };
}
