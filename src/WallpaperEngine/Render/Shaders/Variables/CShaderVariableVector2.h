#pragma once

#include <irrlicht/irrlicht.h>

#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariable.h"

namespace WallpaperEngine::Render::Shaders::Variables
{
    class CShaderVariableVector2 : public CShaderVariable
    {
    public:
        explicit CShaderVariableVector2 (const irr::core::vector2df& defaultValue);
        CShaderVariableVector2 (const irr::core::vector2df& defaultValue, std::string name);

        const int getSize () const override;

        void setValue (const irr::core::vector2df& value);

        static const std::string Type;

    private:
        irr::core::vector2df m_defaultValue;
        irr::core::vector2df m_value;
    };
}
