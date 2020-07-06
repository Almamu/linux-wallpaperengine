#pragma once

#include <irrlicht/irrlicht.h>

#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariable.h"

namespace WallpaperEngine::Render::Shaders::Variables
{
    class CShaderVariableVector4 : public CShaderVariable
    {
    public:
        explicit CShaderVariableVector4 (const irr::core::vector3df& defaultValue);
        CShaderVariableVector4 (const irr::core::vector3df& defaultValue, std::string name);

        const int getSize () const override;

        void setValue (const irr::core::vector3df& value);

        static const std::string Type;

    private:
        irr::core::vector3df m_defaultValue;
        irr::core::vector3df m_value;
    };
}
