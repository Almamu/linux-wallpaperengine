#pragma once

#include <string>

#include <irrlicht/irrlicht.h>

namespace WallpaperEngine::Irrlicht
{
    class CContext
    {
    public:
        void setDevice (irr::IrrlichtDevice* device);

        irr::IrrlichtDevice* getDevice ();
        irr::io::path resolveMaterials (const std::string& materialName);
        irr::io::path resolveVertexShader (const std::string& vertexShader);
        irr::io::path resolveFragmentShader (const std::string& fragmentShader);
        irr::io::path resolveIncludeShader (const std::string& includeShader);
    private:
        irr::io::path resolveFile (const irr::io::path& file);

        irr::IrrlichtDevice* m_device;
    };
};
