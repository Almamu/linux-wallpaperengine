#include "frontends/render.h"

#include "WallpaperEngine/Context.h"
#include "WallpaperEngine/Render/CWallpaper.h"
#include "frontends/project.h"

void wp_render_frame (wp_project* project) {
	static_cast<WallpaperEngine::LoadedProject*> (project)->render->render ();
}