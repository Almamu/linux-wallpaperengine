#pragma once

#include <string>

#include "WallpaperEngine/Desktop/Output.h"

namespace WallpaperEngine::Desktop::X11 {
class Output : public Desktop::Output {
public:
	Output (wp_project* wallpaper, const std::string& name, glm::vec4 viewport);

	std::string name;
};
}
