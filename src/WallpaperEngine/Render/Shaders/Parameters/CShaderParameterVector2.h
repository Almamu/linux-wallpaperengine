#pragma once

#include <irrlicht/irrlicht.h>

#include "WallpaperEngine/Render/Shaders/Parameters/CShaderParameter.h"

namespace WallpaperEngine::Render::Shaders::Parameters
{
    class CShaderParameterVector2 : public CShaderParameter
    {
    public:
        CShaderParameterVector2 (const irr::core::vector2df& defaultValue);

        int getSize () override;

        void setValue (irr::core::vector2df value);

        static const std::string Type;

    private:
        irr::core::vector2df m_defaultValue;
        irr::core::vector2df m_value;
    };
}
