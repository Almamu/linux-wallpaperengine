#pragma once

#include "Render/CWallpaper.h"

#include <filesystem>
#include <memory>
#include <vector>

#include "Render/RenderContext.h"

#include "frontends/project.h"

namespace WallpaperEngine {
class Context;
class Project {
protected:
	std::vector<ProjectUniquePtr>::const_iterator ref;
	std::unique_ptr<CWallpaper> wallpaper;
	std::unique_ptr<RenderContext> renderContext;
	Context& context;

	Project (Context* context, wp_mouse_input* mouse_input, const std::filesystem::path& project);

public:
	~Project ();

	int getWidth () const;
	int getHeight () const;
	void setOutputFramebuffer (const unsigned int framebuffer) const;
	void render ();

	static Project* loadId (Context* context, wp_mouse_input* mouse_input, const int id);
	static Project* loadFolder (Context* context, wp_mouse_input* mouse_input, const char* folder);
};
};