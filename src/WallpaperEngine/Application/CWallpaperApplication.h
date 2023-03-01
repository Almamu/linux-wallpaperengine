#pragma once

#include "CApplicationContext.h"
#include "WallpaperEngine/Render/Drivers/COpenGLDriver.h"
#include "WallpaperEngine/Assets/CCombinedContainer.h"
#include "WallpaperEngine/Core/CProject.h"
#include "WallpaperEngine/Render/CWallpaper.h"

namespace WallpaperEngine::Render
{
    class CWallpaper;
}

namespace WallpaperEngine::Application
{
    using namespace WallpaperEngine::Assets;

    class CWallpaperApplication
    {
    public:
        CWallpaperApplication (CApplicationContext& context);

        void show ();
        void signal (int signal);
        const CApplicationContext& get_context () const;

    private:
        void setupContainer ();
        void loadProject ();
        void setupProperties ();
        void takeScreenshot (WallpaperEngine::Render::CWallpaper* wp, const std::filesystem::path& filename, FREE_IMAGE_FORMAT format);

        Core::CProject* m_project;
        CApplicationContext& m_context;
        CCombinedContainer m_vfs;
    };
}
