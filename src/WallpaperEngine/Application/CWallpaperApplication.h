#pragma once

#include "CApplicationContext.h"
#include "WallpaperEngine/Render/Drivers/COpenGLDriver.h"
#include "WallpaperEngine/Assets/CCombinedContainer.h"
#include "WallpaperEngine/Core/CProject.h"
#include "WallpaperEngine/Render/CWallpaper.h"
#include "WallpaperEngine/Render/CRenderContext.h"

namespace WallpaperEngine::Render
{
    class CWallpaper;
	class CRenderContext;
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
		const std::map <std::string, Core::CProject*>& getProjects () const;
		Core::CProject* getDefaultProject () const;

	private:
        void setupContainer (CCombinedContainer& container, const std::string& bg) const;
		void loadProjects ();
        Core::CProject* loadProject (const std::string& bg);
        void setupProperties ();
		void setupPropertiesForProject (Core::CProject* project);
        void takeScreenshot (const Render::CRenderContext& context, const std::filesystem::path& filename, FREE_IMAGE_FORMAT format);

        Core::CProject* m_defaultProject;
        CApplicationContext& m_context;
		std::map <std::string, Core::CProject*> m_projects;
    };
}
