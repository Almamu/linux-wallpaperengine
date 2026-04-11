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
	std::vector<ProjectSharedPtr>::const_iterator ref;
	std::unique_ptr<CWallpaper> wallpaper;
	std::unique_ptr<RenderContext> renderContext;
	wp_mouse_input* mouse_input;
	Context& context;
	Properties::const_iterator property_it;
	wp_project_property* current_property;
	GLuint framebuffer;

	Project (Context* context, wp_mouse_input* mouse_input, const std::filesystem::path& project);

public:
	~Project ();

	int getWidth () const;
	int getHeight () const;
	void setOutputFramebuffer (GLuint newFramebuffer);
	void render ();

	wp_project_property* propertyListNext ();
	void propertyListReset ();

	void propertySet (const wp_project_property* property, float value);
	void propertySet (const wp_project_property* property, const std::string& value);
	void propertySet (const wp_project_property* property, glm::vec4 value);
	void propertySet (const wp_project_property* property, bool value);
	void propertySet (const std::string& key, const std::string& value);
	void propertySet (const std::string& key, float value);
	void propertySet (const std::string& key, glm::vec4 value);
	void propertySet (const std::string& key, bool value);

	void describe (wp_describe_callback* callback);

	static Project* loadId (Context* context, wp_mouse_input* mouse_input, const int id);
	static Project* loadId (Context* context, wp_mouse_input* mouse_input, const std::string& id);
	static Project* loadFolder (Context* context, wp_mouse_input* mouse_input, const char* folder);
};
};