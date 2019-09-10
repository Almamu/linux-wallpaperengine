#pragma once

#include <vector>
#include <string>

#include <irrlicht/irrlicht.h>

#include "WallpaperEngine/Render/CScene.h"

namespace WallpaperEngine::Render
{
    class CScene;
};

namespace WallpaperEngine::Irrlicht
{
    class CContext
    {
    public:
        CContext (std::vector<std::string>  screens, bool isRootWindow = false);

        void setDevice (irr::IrrlichtDevice* device);
        void initializeContext ();

        void renderFrame (Render::CScene* scene);

        irr::IrrlichtDevice* getDevice ();
        irr::io::path resolveMaterials (const std::string& materialName);
        irr::io::path resolveVertexShader (const std::string& vertexShader);
        irr::io::path resolveFragmentShader (const std::string& fragmentShader);
        irr::io::path resolveIncludeShader (const std::string& includeShader);
    private:
        void initializeViewports (irr::SIrrlichtCreationParameters& irrlichtCreationParameters);

        irr::io::path resolveFile (const irr::io::path& file);

        irr::IrrlichtDevice* m_device;

        std::vector<std::string> m_screens;
        std::vector<irr::core::recti> m_viewports;
        bool m_isRootWindow;
    };
};
