#pragma once

#include "frontends/context.h"

#include "Configuration.h"

#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Model/Wallpaper.h"

#include "Audio/AudioContext.h"
#include "Render/CWallpaper.h"
#include "Render/RenderContext.h"

namespace WallpaperEngine {
struct LoadedProject {
	std::vector<ProjectUniquePtr>::const_iterator ref;
	std::unique_ptr<CWallpaper> wallpaper;
	std::unique_ptr<RenderContext> render;
	Context& context;
};

struct Context {
	std::unique_ptr<TextureCache> texture_cache;
	std::unique_ptr<AudioContext> audio;
	const Configuration* config;
	wp_gl_proc_address* gl_proc_address;
	wp_time_counter* time_counter;
	std::vector<ProjectUniquePtr> projects;
	bool isRunning;
	float renderTime;
	float renderTimeLast;
	float daytime;
};
}