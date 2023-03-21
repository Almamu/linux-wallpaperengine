#pragma once

#include <map>
#include <string>
#include <glm/vec4.hpp>

#include "WallpaperEngine/Application/CApplicationContext.h"

using namespace WallpaperEngine::Application;

namespace WallpaperEngine::Application
{
	class CApplicationContext;
}

namespace WallpaperEngine::Render::Drivers::Output
{
	class COutput
	{
	public:
		COutput (CApplicationContext& context);

		virtual void reset () = 0;

		int getFullWidth () const;
		int getFullHeight () const;

		struct ScreenInfo
		{
			glm::ivec4 viewport;
			std::string name;
		};

		virtual bool renderVFlip () const = 0;
		virtual bool renderMultiple () const = 0;
		virtual bool haveImageBuffer () const = 0;
		const std::map <std::string, ScreenInfo>& getViewports () const;
		virtual void* getImageBuffer () const = 0;
		virtual void updateRender () const = 0;

	protected:
		mutable int m_fullWidth;
		mutable int m_fullHeight;
		mutable std::map <std::string, ScreenInfo> m_viewports;
		CApplicationContext& m_context;
	};
}