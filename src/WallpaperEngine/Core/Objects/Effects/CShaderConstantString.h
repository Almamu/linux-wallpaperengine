#pragma once

#include "CShaderConstant.h"

#include <string>
#include <irrlicht/irrlicht.h>

namespace WallpaperEngine::Core::Objects::Effects
{
    class CShaderConstantString : public CShaderConstant
    {
    public:
        CShaderConstantString (std::string value);

        std::string* getValue ();

    protected:
        std::string m_value;

        static const std::string Type;
    };
}
