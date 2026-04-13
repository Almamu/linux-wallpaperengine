#include "Output.h"

using namespace WallpaperEngine::Desktop::X11;

Output::Output (wp_project* wallpaper, const std::string& name, const glm::vec4 viewport) :
	Desktop::Output (wallpaper, viewport, true) {
	this->name = name;
}