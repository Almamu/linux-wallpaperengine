#pragma once

#include <map>
#include <string>
#include <glm/vec4.hpp>

#include <X11/Xlib.h>

#include "WallpaperEngine/Render/Drivers/CVideoDriver.h"
#include "COutput.h"

namespace WallpaperEngine::Render::Drivers::Output
{
	class CX11Output : public COutput
	{
	public:
		CX11Output (CApplicationContext& context, CVideoDriver& driver);
		~CX11Output ();

		void reset () override;

		bool renderVFlip () const override;
		bool renderMultiple () const override;
		bool haveImageBuffer () const override;
		void* getImageBuffer () const override;
		void updateRender () const override;

	private:
		void loadScreenInfo ();
		void free ();

		Display* m_display;
		Pixmap m_pixmap;
		Window m_root;
		GC m_gc;
		char* m_imageData;
		XImage* m_image;
		CVideoDriver& m_driver;
	};
}