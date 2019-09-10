#pragma once

#include <irrlicht/irrlicht.h>

#include "WallpaperEngine/Render/Shaders/Parameters/CShaderParameter.h"

namespace WallpaperEngine::Render::Shaders::Parameters
{
    class CShaderParameterVector3 : public CShaderParameter
    {
    public:
        CShaderParameterVector3 (const irr::core::vector3df& defaultValue);

        const int getSize () const override;

        void setValue (irr::core::vector3df value);

        static const std::string Type;

    private:
        irr::core::vector3df m_defaultValue;
        irr::core::vector3df m_value;
    };
}
